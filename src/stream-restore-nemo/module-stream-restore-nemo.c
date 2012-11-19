/***
  This file is copied from PulseAudio (the version used in MeeGo 1.2
  Harmattan).

  Copyright 2008 Lennart Poettering
  Copyright 2009 Tanu Kaskinen

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <pulse/xmalloc.h>
#include <pulse/volume.h>
#include <pulse/timeval.h>
#include <pulse/util.h>
#include <pulse/rtclock.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/macro.h>
#include <pulsecore/log.h>
#include <pulsecore/core-subscribe.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/source-output.h>
#include <pulsecore/namereg.h>
#include <pulsecore/protocol-native.h>
#include <pulsecore/pstream.h>
#include <pulsecore/pstream-util.h>
#include <pulsecore/database.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/protocol-dbus.h>

#include "module-stream-restore-nemo-symdef.h"
#include "volume-proxy.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Automatically restore the volume/mute/device state of streams");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(TRUE);
PA_MODULE_USAGE(
        "restore_device=<Save/restore sinks/sources?> "
        "restore_volume=<Save/restore volumes?> "
        "restore_muted=<Save/restore muted states?> "
        "on_hotplug=<When new device becomes available, recheck streams?> "
        "on_rescue=<When device becomes unavailable, recheck streams?> "
        "fallback_table=<filename> "
        "route_table=<filename> "
        "sink_volume_table=<filename>");

#define SAVE_INTERVAL (10 * PA_USEC_PER_SEC)
#define IDENTIFICATION_PROPERTY "module-stream-restore.id"

#define DEFAULT_FALLBACK_FILE PA_DEFAULT_CONFIG_DIR"/stream-restore.table"
#define DEFAULT_FALLBACK_FILE_USER "stream-restore.table"

#define DEFAULT_ROUTE_FILE PA_DEFAULT_CONFIG_DIR"/route-entry.table"
#define DEFAULT_ROUTE_FILE_USER "route-entry.table"

/* sink-volume table syntax:
 * <audio mode>:<sink to control>
 * for example:
 * btmono:sink.hw1
 */
#define DEFAULT_SINK_VOLUME_FILE PA_DEFAULT_CONFIG_DIR"/sink-volume.table"
#define DEFAULT_SINK_VOLUME_FILE_USER "sink-volume.table"

#define WHITESPACE "\n\r \t"

#define PA_NOKIA_PROP_AUDIO_MODE "x-maemo.mode"
#define VOICE_MASTER_SINK_INPUT_NAME "Voice module master sink input"

static const char* const valid_modargs[] = {
    "restore_device",
    "restore_volume",
    "restore_muted",
    "restore_route_volume",
    "on_hotplug",
    "on_rescue",
    "fallback_table",
    "route_table",
    "sink_volume_table",
    NULL
};

struct route_volume {
    char *name;
    pa_cvolume volume;
    pa_cvolume min_volume;
    pa_cvolume default_volume;
    pa_bool_t reset_min_volume;

    /* when "slave" route volume enabled stream is changed, master is set to
     * same volume, and when setting master, also slaves are updated. */
    struct route_volume *master;

    PA_LLIST_FIELDS(struct route_volume);
};

struct sink_volume {
    char *mode;
    char *sink_name;
    pa_sink *sink;

    PA_LLIST_FIELDS(struct sink_volume);
};

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_subscription *subscription;
    pa_hook_slot
        *sink_input_new_hook_slot,
        *sink_input_fixate_hook_slot,
        *source_output_new_hook_slot,
        *sink_put_hook_slot,
        *source_put_hook_slot,
        *sink_unlink_hook_slot,
        *source_unlink_hook_slot,
        *connection_unlink_hook_slot;
    pa_time_event *save_time_event;
    pa_database* database;

    pa_bool_t restore_device:1;
    pa_bool_t restore_volume:1;
    pa_bool_t restore_muted:1;
    pa_bool_t restore_route_volume:1;
    pa_bool_t on_hotplug:1;
    pa_bool_t on_rescue:1;

    pa_native_protocol *protocol;
    pa_idxset *subscribed;

    pa_hook_slot *sink_proplist_changed_slot;
    pa_hook_slot *sink_input_move_finished_slot;
    pa_database *route_database;
    char *route;

    pa_volume_proxy *volume_proxy;
    pa_hook_slot *volume_proxy_hook_slot;

    pa_dbus_protocol *dbus_protocol;
    pa_hashmap *dbus_entries;
    uint32_t next_index; /* For generating object paths for entries. */

    PA_LLIST_HEAD(struct route_volume, route_volumes);

    /* sink volumes */
    pa_subscription *sink_subscription;
    struct sink_volume *use_sink_volume;
    PA_LLIST_HEAD(struct sink_volume, sink_volumes);
};

#define ENTRY_VERSION 4
struct entry {
    uint8_t version;
    pa_bool_t muted_valid:1, volume_valid:1, device_valid:1, card_valid:1;
    pa_bool_t muted:1;
    pa_channel_map channel_map;
    pa_cvolume volume;
    char device[PA_NAME_MAX];
    char card[PA_NAME_MAX];
};

#define ROUTE_ENTRY_VERSION 4
struct route_entry {
    uint8_t version;
    pa_cvolume volume;
};

enum {
    SUBCOMMAND_TEST,
    SUBCOMMAND_READ,
    SUBCOMMAND_WRITE,
    SUBCOMMAND_DELETE,
    SUBCOMMAND_SUBSCRIBE,
    SUBCOMMAND_EVENT
};

static struct entry *read_entry(struct userdata *u, const char *name);
static pa_bool_t entries_equal(const struct entry *a, const struct entry *b);
static void apply_entry(struct userdata *u, const char *name, struct entry *e);
static void trigger_save(struct userdata *u);
static char *get_name(pa_proplist *p, const char *prefix);
static void sink_volume_subscribe_cb(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata);
static void subscribe_callback(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata);
static void _apply_route_volumes(struct userdata *u, pa_bool_t apply);

static void _sink_set_volume(pa_sink *s, const pa_cvolume *vol) {
    pa_channel_map c;
    pa_cvolume remapped;

    pa_assert(s);
    pa_assert(vol);
    pa_assert(vol->channels == 1 || vol->channels == 2);

    if (vol->channels == 1)
        pa_channel_map_init_mono(&c);
    else
        pa_channel_map_init_stereo(&c);

    remapped = *vol;
    pa_sink_set_volume(s,
                       pa_cvolume_remap(&remapped, &c, &s->channel_map),
                       FALSE,
                       FALSE);
}

static struct sink_volume* have_sink_volume(struct userdata *u, const char *mode) {
    struct sink_volume *v = NULL;

    pa_assert(u);
    pa_assert(mode);

    for (v = u->sink_volumes; v; v = v->next) {
        if (pa_streq(mode, v->mode)) {
            if (!v->sink)
                v->sink = pa_namereg_get(u->core, v->sink_name, PA_NAMEREG_SINK);

            if (v->sink)
                return v;
            else
                return NULL;
        }
    }

    return NULL;
}

static void _free_sink_volumes(struct userdata *u) {
    struct sink_volume *v;

    pa_assert(u);

    while ((v = u->sink_volumes)) {
        PA_LLIST_REMOVE(struct sink_volume, u->sink_volumes, v);
        pa_xfree(v->mode);
        pa_xfree(v->sink_name);
        pa_xfree(v);
    }
}

/* return struct route_volume* when entry with given
 * name exists, otherwise return NULL */
static struct route_volume* get_route_volume_by_name(struct userdata *u, const char *name) {
    struct route_volume *ret = NULL;
    struct route_volume *r;

    PA_LLIST_FOREACH(r, u->route_volumes) {
        if (pa_streq(name, r->name)) {
            ret = r;
            break;
        }
    }

    return ret;
}

static void set_route_volume(struct route_volume *r, const pa_cvolume *volume) {
    pa_assert(r);
    pa_assert(pa_cvolume_valid(volume));

    r->volume = *volume;
}

static void set_route_volume_by_name(struct userdata *u, const char *name, const pa_cvolume *volume) {
    struct route_volume *r;

    pa_assert(u);
    pa_assert(name);
    pa_assert(pa_cvolume_valid(volume));

    if (!u->route)
        return;

    if ((r = get_route_volume_by_name(u, name)))
        set_route_volume(r, volume);
}

static void set_route_volumes(struct userdata *u, const pa_cvolume *volume) {
    struct route_volume *r;

    pa_assert(u);
    pa_assert(volume);
    pa_assert(pa_cvolume_valid(volume));

    PA_LLIST_FOREACH(r, u->route_volumes)
        set_route_volume(r, volume);
}

static void _set_stream(struct userdata *u, const char *name, const pa_volume_t volume, const int muted) {
    pa_sink_input *si;
    uint32_t idx;
    pa_channel_map from;
    pa_cvolume vol;

    pa_assert(u);
    pa_assert(name);

    pa_cvolume_init(&vol);
    pa_channel_map_init_mono(&from);

    for (si = pa_idxset_first(u->core->sink_inputs, &idx); si; si = pa_idxset_next(u->core->sink_inputs, &idx)) {
        char *n;

        if (!si->sink) /* for eg. moving */
            continue;

        if (!(n = get_name(si->proplist, "sink-input")))
            continue;

        if (!pa_streq(name, n)) {
            pa_xfree(n);
            continue;
        }
        pa_xfree(n);

        if (si->volume_writable) {
            pa_log_info("Restoring volume for sink input %s. c %d vol %d", name, from.channels, volume);
            pa_cvolume_set(&vol, 1, volume);
            pa_sink_input_set_volume(si, pa_cvolume_remap(&vol, &from, &si->channel_map), TRUE, TRUE);
        }

        if (muted > 0) {
            pa_log_info("Restoring mute state for sink input %s.", name);
            pa_sink_input_set_mute(si, !!muted, TRUE);
        }

    }
}

static void _set_streams(struct userdata *u, const pa_volume_t volume, const int muted) {
    struct route_volume *r;

    pa_assert(u);

    for (r = u->route_volumes; r; r = r->next) {
        _set_stream(u, r->name, volume, muted);
    }
}

static void _proxy_volume(struct userdata *u, const char*name, const pa_cvolume *volume) {
    pa_assert(u);
    pa_assert(u->volume_proxy);
    pa_assert(name);
    pa_assert(volume);
    pa_assert(pa_cvolume_valid(volume));

    pa_volume_proxy_set_volume(u->volume_proxy, name, pa_cvolume_max(volume));
}

static void proxy_volume_all(struct userdata *u) {
    struct route_volume *r;

    pa_assert(u);
    pa_assert(u->volume_proxy);

    /* proxy all route volumes */
    PA_LLIST_FOREACH(r, u->route_volumes)
        _proxy_volume(u, r->name, &r->volume);
}

static pa_hook_result_t volume_proxy_cb(pa_volume_proxy *p, const char *name, struct userdata *u) {
    pa_volume_t vol;
    pa_cvolume volume;
    struct route_volume *r;
    pa_bool_t changed = FALSE;

    if (!pa_volume_proxy_get_volume(p, name, &vol))
        return PA_HOOK_OK;

    if ((r = get_route_volume_by_name(u, name))) {
        int i;
        for (i = 0; i < r->volume.channels; i++) {
            if (vol != r->volume.values[i]) {
                pa_cvolume_set(&r->volume, r->volume.channels, vol);
                changed = TRUE;
                break;
            }
        }
    }

    if (changed) {
        if (u->use_sink_volume) {
            pa_log_debug("volume_proxy_cb() adjust sink-volume %u", vol);

            pa_cvolume_init(&volume);
            pa_cvolume_set(&volume, 1, vol);

            /* set all route volumes to sink volume */
            set_route_volumes(u, &volume);
            _sink_set_volume(u->use_sink_volume->sink, &volume);
            _apply_route_volumes(u, FALSE);
            /* trigger_save() is going to be called in sink_volume_subscribe_cb */
        } else {
            _apply_route_volumes(u, TRUE);
            trigger_save(u);
        }
    }

    return PA_HOOK_OK;
}

#define OBJECT_PATH "/org/pulseaudio/stream_restore1"
#define ENTRY_OBJECT_NAME "entry"
#define INTERFACE_STREAM_RESTORE "org.PulseAudio.Ext.StreamRestore1"
#define INTERFACE_ENTRY INTERFACE_STREAM_RESTORE ".RestoreEntry"

#define DBUS_INTERFACE_REVISION 0

struct dbus_entry {
    struct userdata *userdata;

    char *entry_name;
    uint32_t index;
    char *object_path;
};

static void handle_get_interface_revision(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_entries(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_add_entry(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_get_entry_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_entry_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_entry_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_entry_get_device(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_entry_set_device(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_entry_get_volume(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_entry_set_volume(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);
static void handle_entry_get_mute(DBusConnection *conn, DBusMessage *msg, void *userdata);
static void handle_entry_set_mute(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata);

static void handle_entry_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata);

static void handle_entry_remove(DBusConnection *conn, DBusMessage *msg, void *userdata);

enum property_handler_index {
    PROPERTY_HANDLER_INTERFACE_REVISION,
    PROPERTY_HANDLER_ENTRIES,
    PROPERTY_HANDLER_MAX
};

enum entry_property_handler_index {
    ENTRY_PROPERTY_HANDLER_INDEX,
    ENTRY_PROPERTY_HANDLER_NAME,
    ENTRY_PROPERTY_HANDLER_DEVICE,
    ENTRY_PROPERTY_HANDLER_VOLUME,
    ENTRY_PROPERTY_HANDLER_MUTE,
    ENTRY_PROPERTY_HANDLER_MAX
};

static pa_dbus_property_handler property_handlers[PROPERTY_HANDLER_MAX] = {
    [PROPERTY_HANDLER_INTERFACE_REVISION] = { .property_name = "InterfaceRevision", .type = "u",  .get_cb = handle_get_interface_revision, .set_cb = NULL },
    [PROPERTY_HANDLER_ENTRIES]            = { .property_name = "Entries",           .type = "ao", .get_cb = handle_get_entries,            .set_cb = NULL }
};

static pa_dbus_property_handler entry_property_handlers[ENTRY_PROPERTY_HANDLER_MAX] = {
    [ENTRY_PROPERTY_HANDLER_INDEX]    = { .property_name = "Index",   .type = "u",     .get_cb = handle_entry_get_index,    .set_cb = NULL },
    [ENTRY_PROPERTY_HANDLER_NAME]     = { .property_name = "Name",    .type = "s",     .get_cb = handle_entry_get_name,     .set_cb = NULL },
    [ENTRY_PROPERTY_HANDLER_DEVICE]   = { .property_name = "Device",  .type = "s",     .get_cb = handle_entry_get_device,   .set_cb = handle_entry_set_device },
    [ENTRY_PROPERTY_HANDLER_VOLUME]   = { .property_name = "Volume",  .type = "a(uu)", .get_cb = handle_entry_get_volume,   .set_cb = handle_entry_set_volume },
    [ENTRY_PROPERTY_HANDLER_MUTE]     = { .property_name = "Mute",    .type = "b",     .get_cb = handle_entry_get_mute,     .set_cb = handle_entry_set_mute }
};

enum method_handler_index {
    METHOD_HANDLER_ADD_ENTRY,
    METHOD_HANDLER_GET_ENTRY_BY_NAME,
    METHOD_HANDLER_MAX
};

enum entry_method_handler_index {
    ENTRY_METHOD_HANDLER_REMOVE,
    ENTRY_METHOD_HANDLER_MAX
};

static pa_dbus_arg_info add_entry_args[] = { { "name",              "s",     "in" },
                                             { "device",            "s",     "in" },
                                             { "volume",            "a(uu)", "in" },
                                             { "mute",              "b",     "in" },
                                             { "apply_immediately", "b",     "in" },
                                             { "entry",             "o",     "out" } };
static pa_dbus_arg_info get_entry_by_name_args[] = { { "name", "s", "in" }, { "entry", "o", "out" } };

static pa_dbus_method_handler method_handlers[METHOD_HANDLER_MAX] = {
    [METHOD_HANDLER_ADD_ENTRY] = {
        .method_name = "AddEntry",
        .arguments = add_entry_args,
        .n_arguments = sizeof(add_entry_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_add_entry },
    [METHOD_HANDLER_GET_ENTRY_BY_NAME] = {
        .method_name = "GetEntryByName",
        .arguments = get_entry_by_name_args,
        .n_arguments = sizeof(get_entry_by_name_args) / sizeof(pa_dbus_arg_info),
        .receive_cb = handle_get_entry_by_name }
};

static pa_dbus_method_handler entry_method_handlers[ENTRY_METHOD_HANDLER_MAX] = {
    [ENTRY_METHOD_HANDLER_REMOVE] = {
        .method_name = "Remove",
        .arguments = NULL,
        .n_arguments = 0,
        .receive_cb = handle_entry_remove }
};

enum signal_index {
    SIGNAL_NEW_ENTRY,
    SIGNAL_ENTRY_REMOVED,
    SIGNAL_MAX
};

enum entry_signal_index {
    ENTRY_SIGNAL_DEVICE_UPDATED,
    ENTRY_SIGNAL_VOLUME_UPDATED,
    ENTRY_SIGNAL_MUTE_UPDATED,
    ENTRY_SIGNAL_MAX
};

static pa_dbus_arg_info new_entry_args[]     = { { "entry", "o", NULL } };
static pa_dbus_arg_info entry_removed_args[] = { { "entry", "o", NULL } };

static pa_dbus_arg_info entry_device_updated_args[] = { { "device", "s",     NULL } };
static pa_dbus_arg_info entry_volume_updated_args[] = { { "volume", "a(uu)", NULL } };
static pa_dbus_arg_info entry_mute_updated_args[]   = { { "muted",  "b",     NULL } };

static pa_dbus_signal_info signals[SIGNAL_MAX] = {
    [SIGNAL_NEW_ENTRY]     = { .name = "NewEntry",     .arguments = new_entry_args,     .n_arguments = 1 },
    [SIGNAL_ENTRY_REMOVED] = { .name = "EntryRemoved", .arguments = entry_removed_args, .n_arguments = 1 }
};

static pa_dbus_signal_info entry_signals[ENTRY_SIGNAL_MAX] = {
    [ENTRY_SIGNAL_DEVICE_UPDATED] = { .name = "DeviceUpdated", .arguments = entry_device_updated_args, .n_arguments = 1 },
    [ENTRY_SIGNAL_VOLUME_UPDATED] = { .name = "VolumeUpdated", .arguments = entry_volume_updated_args, .n_arguments = 1 },
    [ENTRY_SIGNAL_MUTE_UPDATED]   = { .name = "MuteUpdated",   .arguments = entry_mute_updated_args,   .n_arguments = 1 }
};

static pa_dbus_interface_info stream_restore_interface_info = {
    .name = INTERFACE_STREAM_RESTORE,
    .method_handlers = method_handlers,
    .n_method_handlers = METHOD_HANDLER_MAX,
    .property_handlers = property_handlers,
    .n_property_handlers = PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_get_all,
    .signals = signals,
    .n_signals = SIGNAL_MAX
};

static pa_dbus_interface_info entry_interface_info = {
    .name = INTERFACE_ENTRY,
    .method_handlers = entry_method_handlers,
    .n_method_handlers = ENTRY_METHOD_HANDLER_MAX,
    .property_handlers = entry_property_handlers,
    .n_property_handlers = ENTRY_PROPERTY_HANDLER_MAX,
    .get_all_properties_cb = handle_entry_get_all,
    .signals = entry_signals,
    .n_signals = ENTRY_SIGNAL_MAX
};

static struct dbus_entry *dbus_entry_new(struct userdata *u, const char *entry_name) {
    struct dbus_entry *de;

    pa_assert(u);
    pa_assert(entry_name);
    pa_assert(*entry_name);

    de = pa_xnew(struct dbus_entry, 1);
    de->userdata = u;
    de->entry_name = pa_xstrdup(entry_name);
    de->index = u->next_index++;
    de->object_path = pa_sprintf_malloc("%s/%s%u", OBJECT_PATH, ENTRY_OBJECT_NAME, de->index);

    pa_assert_se(pa_dbus_protocol_add_interface(u->dbus_protocol, de->object_path, &entry_interface_info, de) >= 0);

    return de;
}

static void dbus_entry_free(struct dbus_entry *de) {
    pa_assert(de);

    pa_assert_se(pa_dbus_protocol_remove_interface(de->userdata->dbus_protocol, de->object_path, entry_interface_info.name) >= 0);

    pa_xfree(de->entry_name);
    pa_xfree(de->object_path);
    pa_xfree(de);
}

/* Reads an array [(UInt32, UInt32)] from the iterator. The struct items are
 * are a channel position and a volume value, respectively. The result is
 * stored in the map and vol arguments. The iterator must point to a "a(uu)"
 * element. If the data is invalid, an error reply is sent and a negative
 * number is returned. In case of a failure we make no guarantees about the
 * state of map and vol. In case of an empty array the channels field of both
 * map and vol are set to 0. This function calls dbus_message_iter_next(iter)
 * before returning. */
static int get_volume_arg(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, pa_channel_map *map, pa_cvolume *vol) {
    DBusMessageIter array_iter;
    DBusMessageIter struct_iter;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(pa_streq(dbus_message_iter_get_signature(iter), "a(uu)"));
    pa_assert(map);
    pa_assert(vol);

    pa_channel_map_init(map);
    pa_cvolume_init(vol);

    map->channels = 0;
    vol->channels = 0;

    dbus_message_iter_recurse(iter, &array_iter);

    while (dbus_message_iter_get_arg_type(&array_iter) != DBUS_TYPE_INVALID) {
        dbus_uint32_t chan_pos;
        dbus_uint32_t chan_vol;

        dbus_message_iter_recurse(&array_iter, &struct_iter);

        dbus_message_iter_get_basic(&struct_iter, &chan_pos);

        if (chan_pos >= PA_CHANNEL_POSITION_MAX) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid channel position: %u", chan_pos);
            return -1;
        }

        pa_assert_se(dbus_message_iter_next(&struct_iter));
        dbus_message_iter_get_basic(&struct_iter, &chan_vol);

        if (chan_vol > PA_VOLUME_MAX) {
            pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Invalid volume: %u", chan_vol);
            return -1;
        }

        if (map->channels < PA_CHANNELS_MAX) {
            map->map[map->channels] = chan_pos;
            vol->values[map->channels] = chan_vol;
        }
        ++map->channels;
        ++vol->channels;

        dbus_message_iter_next(&array_iter);
    }

    if (map->channels > PA_CHANNELS_MAX) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Too many channels: %u. The maximum is %u.", map->channels, PA_CHANNELS_MAX);
        return -1;
    }

    dbus_message_iter_next(iter);

    return 0;
}

static void append_volume(DBusMessageIter *iter, struct entry *e) {
    DBusMessageIter array_iter;
    DBusMessageIter struct_iter;
    unsigned i;

    pa_assert(iter);
    pa_assert(e);

    pa_assert_se(dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY, "(uu)", &array_iter));

    if (!e->volume_valid) {
        pa_assert_se(dbus_message_iter_close_container(iter, &array_iter));
        return;
    }

    for (i = 0; i < e->channel_map.channels; ++i) {
        pa_assert_se(dbus_message_iter_open_container(&array_iter, DBUS_TYPE_STRUCT, NULL, &struct_iter));

        pa_assert_se(dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_UINT32, &e->channel_map.map[i]));
        pa_assert_se(dbus_message_iter_append_basic(&struct_iter, DBUS_TYPE_UINT32, &e->volume.values[i]));

        pa_assert_se(dbus_message_iter_close_container(&array_iter, &struct_iter));
    }

    pa_assert_se(dbus_message_iter_close_container(iter, &array_iter));
}

static void append_volume_variant(DBusMessageIter *iter, struct entry *e) {
    DBusMessageIter variant_iter;

    pa_assert(iter);
    pa_assert(e);

    pa_assert_se(dbus_message_iter_open_container(iter, DBUS_TYPE_VARIANT, "a(uu)", &variant_iter));

    append_volume(&variant_iter, e);

    pa_assert_se(dbus_message_iter_close_container(iter, &variant_iter));
}

static void send_new_entry_signal(struct dbus_entry *entry) {
    DBusMessage *signal_msg;

    pa_assert(entry);

    pa_assert_se(signal_msg = dbus_message_new_signal(OBJECT_PATH, INTERFACE_STREAM_RESTORE, signals[SIGNAL_NEW_ENTRY].name));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &entry->object_path, DBUS_TYPE_INVALID));
    pa_dbus_protocol_send_signal(entry->userdata->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);
}

static void send_entry_removed_signal(struct dbus_entry *entry) {
    DBusMessage *signal_msg;

    pa_assert(entry);

    pa_assert_se(signal_msg = dbus_message_new_signal(OBJECT_PATH, INTERFACE_STREAM_RESTORE, signals[SIGNAL_ENTRY_REMOVED].name));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_OBJECT_PATH, &entry->object_path, DBUS_TYPE_INVALID));
    pa_dbus_protocol_send_signal(entry->userdata->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);
}

static void send_device_updated_signal(struct dbus_entry *de, struct entry *e) {
    DBusMessage *signal_msg;
    const char *device;

    pa_assert(de);
    pa_assert(e);

    device = e->device_valid ? e->device : "";

    pa_assert_se(signal_msg = dbus_message_new_signal(de->object_path, INTERFACE_ENTRY, entry_signals[ENTRY_SIGNAL_DEVICE_UPDATED].name));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_STRING, &device, DBUS_TYPE_INVALID));
    pa_dbus_protocol_send_signal(de->userdata->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);
}

static void send_volume_updated_signal(struct dbus_entry *de, struct entry *e) {
    DBusMessage *signal_msg;
    DBusMessageIter msg_iter;

    pa_assert(de);
    pa_assert(e);

    pa_assert_se(signal_msg = dbus_message_new_signal(de->object_path, INTERFACE_ENTRY, entry_signals[ENTRY_SIGNAL_VOLUME_UPDATED].name));
    dbus_message_iter_init_append(signal_msg, &msg_iter);
    append_volume(&msg_iter, e);
    pa_dbus_protocol_send_signal(de->userdata->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);
}

static void send_mute_updated_signal(struct dbus_entry *de, struct entry *e) {
    DBusMessage *signal_msg;
    dbus_bool_t muted;

    pa_assert(de);
    pa_assert(e);

    pa_assert(e->muted_valid);

    muted = e->muted;

    pa_assert_se(signal_msg = dbus_message_new_signal(de->object_path, INTERFACE_ENTRY, entry_signals[ENTRY_SIGNAL_MUTE_UPDATED].name));
    pa_assert_se(dbus_message_append_args(signal_msg, DBUS_TYPE_BOOLEAN, &muted, DBUS_TYPE_INVALID));
    pa_dbus_protocol_send_signal(de->userdata->dbus_protocol, signal_msg);
    dbus_message_unref(signal_msg);
}

static void handle_get_interface_revision(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    dbus_uint32_t interface_revision = DBUS_INTERFACE_REVISION;

    pa_assert(conn);
    pa_assert(msg);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &interface_revision);
}

/* The caller frees the array, but not the strings. */
static const char **get_entries(struct userdata *u, unsigned *n) {
    const char **entries;
    unsigned i = 0;
    void *state = NULL;
    struct dbus_entry *de;

    pa_assert(u);
    pa_assert(n);

    *n = pa_hashmap_size(u->dbus_entries);

    if (*n == 0)
        return NULL;

    entries = pa_xnew(const char *, *n);

    PA_HASHMAP_FOREACH(de, u->dbus_entries, state)
        entries[i++] = de->object_path;

    return entries;
}

static void handle_get_entries(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct userdata *u = userdata;
    const char **entries;
    unsigned n;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    entries = get_entries(u, &n);

    pa_dbus_send_basic_array_variant_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, entries, n);

    pa_xfree(entries);
}

static void handle_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct userdata *u = userdata;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    dbus_uint32_t interface_revision;
    const char **entries;
    unsigned n_entries;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    interface_revision = DBUS_INTERFACE_REVISION;
    entries = get_entries(u, &n_entries);

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_INTERFACE_REVISION].property_name, DBUS_TYPE_UINT32, &interface_revision);
    pa_dbus_append_basic_array_variant_dict_entry(&dict_iter, property_handlers[PROPERTY_HANDLER_ENTRIES].property_name, DBUS_TYPE_OBJECT_PATH, entries, n_entries);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);

    pa_xfree(entries);
}

static void handle_add_entry(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct userdata *u = userdata;
    DBusMessageIter msg_iter;
    const char *name = NULL;
    const char *device = NULL;
    pa_channel_map map;
    pa_cvolume vol;
    dbus_bool_t muted = FALSE;
    dbus_bool_t apply_immediately = FALSE;
    pa_datum key;
    pa_datum value;
    struct dbus_entry *dbus_entry = NULL;
    struct entry *e = NULL;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    pa_assert_se(dbus_message_iter_init(msg, &msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &name);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &device);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    if (get_volume_arg(conn, msg, &msg_iter, &map, &vol) < 0)
        return;

    dbus_message_iter_get_basic(&msg_iter, &muted);

    pa_assert_se(dbus_message_iter_next(&msg_iter));
    dbus_message_iter_get_basic(&msg_iter, &apply_immediately);

    if (!*name) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "An empty string was given as the entry name.");
        return;
    }

    if ((dbus_entry = pa_hashmap_get(u->dbus_entries, name))) {
        pa_bool_t mute_updated = FALSE;
        pa_bool_t volume_updated = FALSE;
        pa_bool_t device_updated = FALSE;

        pa_assert_se(e = read_entry(u, name));
        mute_updated = e->muted != muted;
        e->muted = muted;
        e->muted_valid = TRUE;

        volume_updated = (e->volume_valid != !!map.channels) ||
                         (e->volume_valid && (!pa_cvolume_equal(&e->volume, &vol) ||
                                              !pa_channel_map_equal(&e->channel_map, &map)));
        e->volume = vol;
        e->channel_map = map;
        e->volume_valid = !!map.channels;

        device_updated = (e->device_valid != !!device[0]) || !pa_streq(e->device, device);
        pa_strlcpy(e->device, device, sizeof(e->device));
        e->device_valid = !!device[0];

        set_route_volume_by_name(u, name, &e->volume);

        if (mute_updated)
            send_mute_updated_signal(dbus_entry, e);
        if (volume_updated)
            send_volume_updated_signal(dbus_entry, e);
        if (device_updated)
            send_device_updated_signal(dbus_entry, e);

    } else {
        dbus_entry = dbus_entry_new(u, name);
        pa_assert_se(pa_hashmap_put(u->dbus_entries, dbus_entry->entry_name, dbus_entry) == 0);

        e = pa_xnew0(struct entry, 1);
        e->version = ENTRY_VERSION;
        e->muted_valid = TRUE;
        e->volume_valid = !!map.channels;
        e->device_valid = !!device[0];
        e->muted = muted;
        e->volume = vol;
        e->channel_map = map;
        pa_strlcpy(e->device, device, sizeof(e->device));

        send_new_entry_signal(dbus_entry);
    }

    key.data = (char *) name;
    key.size = strlen(name);

    value.data = e;
    value.size = sizeof(struct entry);

    pa_assert_se(pa_database_set(u->database, &key, &value, TRUE) == 0);

    /* FIXME: If the entry was not new, and its volume was updated, and there
     * is a route volume for the entry, and the new volume wasn't the same for
     * every channel, then the volume will be applied immediately even if
     * apply_immediately is false here. That happens because when the new
     * volume is stored in the volume proxy, it is stored as a mono value only,
     * and when we get a callback from the proxy due to updating it,
     * volume_proxy_cb thinks that the volume has been changed, even though it
     * really isn't (information is lost in the mono transformation).
     * volume_proxy_cb then applies the new mono volume from the volume proxy
     * immediately for existing streams also.
     *
     * If all channels have the same volume to begin with, this doesn't happen,
     * because volume_proxy_cb correctly detects that nothing has changed and
     * nothing needs to be done.
     *
     * This can be fixed either by changing the volume proxy to store
     * multichannel volumes or by changing the route volumes to only store mono
     * volumes. */
    if (apply_immediately)
        apply_entry(u, name, e);

    trigger_save(u);

    if (e->volume_valid)
        _proxy_volume(u, name, &e->volume);

    pa_dbus_send_empty_reply(conn, msg);

    pa_xfree(e);
}

static void handle_get_entry_by_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct userdata *u = userdata;
    const char *name;
    struct dbus_entry *de;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    pa_assert_se(dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_INVALID));

    if (!(de = pa_hashmap_get(u->dbus_entries, name))) {
        pa_dbus_send_error(conn, msg, PA_DBUS_ERROR_NOT_FOUND, "No such stream restore entry.");
        return;
    }

    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_OBJECT_PATH, &de->object_path);
}

static void handle_entry_get_index(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct dbus_entry *de = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(de);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &de->index);
}

static void handle_entry_get_name(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct dbus_entry *de = userdata;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(de);

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &de->entry_name);
}

static void handle_entry_get_device(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct dbus_entry *de = userdata;
    struct entry *e;
    const char *device;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(de);

    pa_assert_se(e = read_entry(de->userdata, de->entry_name));

    device = e->device_valid ? e->device : "";

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_STRING, &device);

    pa_xfree(e);
}

static void handle_entry_set_device(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    struct dbus_entry *de = userdata;
    const char *device;
    struct entry *e;
    pa_bool_t updated;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(de);

    dbus_message_iter_get_basic(iter, &device);

    pa_assert_se(e = read_entry(de->userdata, de->entry_name));

    updated = (e->device_valid != !!device[0]) || !pa_streq(e->device, device);

    if (updated) {
        pa_datum key;
        pa_datum value;

        pa_strlcpy(e->device, device, sizeof(e->device));
        e->device_valid = !!device[0];

        key.data = de->entry_name;
        key.size = strlen(de->entry_name);
        value.data = e;
        value.size = sizeof(struct entry);
        pa_assert_se(pa_database_set(de->userdata->database, &key, &value, TRUE) == 0);

        apply_entry(de->userdata, de->entry_name, e);
        send_device_updated_signal(de, e);
        trigger_save(de->userdata);
    }

    pa_dbus_send_empty_reply(conn, msg);

    pa_xfree(e);
}

static void handle_entry_get_volume(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct dbus_entry *de = userdata;
    DBusMessage *reply;
    DBusMessageIter msg_iter;
    struct entry *e;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(de);

    pa_assert_se(e = read_entry(de->userdata, de->entry_name));

    pa_assert_se(reply = dbus_message_new_method_return(msg));

    dbus_message_iter_init_append(reply, &msg_iter);
    append_volume_variant(&msg_iter, e);

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    pa_xfree(e);
}

static void handle_entry_set_volume(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    struct dbus_entry *de = userdata;
    pa_channel_map map;
    pa_cvolume vol;
    struct entry *e = NULL;
    struct route_volume *r;
    pa_bool_t updated = FALSE;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(de);

    if (get_volume_arg(conn, msg, iter, &map, &vol) < 0)
        return;

    pa_assert_se(e = read_entry(de->userdata, de->entry_name));

    updated = (e->volume_valid != !!map.channels) ||
              (e->volume_valid && (!pa_cvolume_equal(&e->volume, &vol) ||
                                   !pa_channel_map_equal(&e->channel_map, &map)));

    if (updated) {
        pa_datum key;
        pa_datum value;

        e->volume = vol;
        e->channel_map = map;
        e->volume_valid = !!map.channels;

        /* When sink-volume mode is enabled, only update sink volume if route-volume
         * entry volume is modified. */
        if ((r = get_route_volume_by_name(de->userdata, de->entry_name))) {
            if (de->userdata->use_sink_volume) {
                set_route_volumes(de->userdata, &e->volume);
                _sink_set_volume(de->userdata->use_sink_volume->sink, &e->volume);
            } else {
                set_route_volume(r, &e->volume);
            }
        }

        key.data = de->entry_name;
        key.size = strlen(de->entry_name);
        value.data = e;
        value.size = sizeof(struct entry);
        pa_assert_se(pa_database_set(de->userdata->database, &key, &value, TRUE) == 0);

        if (!de->userdata->use_sink_volume) {
            apply_entry(de->userdata, de->entry_name, e);
            trigger_save(de->userdata);
        }

        if (e->volume_valid)
            _proxy_volume(de->userdata, de->entry_name, &e->volume);

        send_volume_updated_signal(de, e);
    }

    pa_dbus_send_empty_reply(conn, msg);

    pa_xfree(e);
}

static void handle_entry_get_mute(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct dbus_entry *de = userdata;
    struct entry *e;
    dbus_bool_t mute;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(de);

    pa_assert_se(e = read_entry(de->userdata, de->entry_name));

    mute = e->muted_valid ? e->muted : FALSE;

    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_BOOLEAN, &mute);

    pa_xfree(e);
}

static void handle_entry_set_mute(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *userdata) {
    struct dbus_entry *de = userdata;
    dbus_bool_t mute;
    struct entry *e;
    pa_bool_t updated;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(iter);
    pa_assert(de);

    dbus_message_iter_get_basic(iter, &mute);

    pa_assert_se(e = read_entry(de->userdata, de->entry_name));

    updated = !e->muted_valid || e->muted != mute;

    if (updated) {
        pa_datum key;
        pa_datum value;

        e->muted = mute;
        e->muted_valid = TRUE;

        key.data = de->entry_name;
        key.size = strlen(de->entry_name);
        value.data = e;
        value.size = sizeof(struct entry);
        pa_assert_se(pa_database_set(de->userdata->database, &key, &value, TRUE) == 0);

        apply_entry(de->userdata, de->entry_name, e);
        send_mute_updated_signal(de, e);
        trigger_save(de->userdata);
    }

    pa_dbus_send_empty_reply(conn, msg);

    pa_xfree(e);
}

static void handle_entry_get_all(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct dbus_entry *de = userdata;
    struct entry *e;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    DBusMessageIter dict_entry_iter;
    const char *device;
    dbus_bool_t mute;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(de);

    pa_assert_se(e = read_entry(de->userdata, de->entry_name));

    device = e->device_valid ? e->device : "";
    mute = e->muted_valid ? e->muted : FALSE;

    pa_assert_se((reply = dbus_message_new_method_return(msg)));

    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, entry_property_handlers[ENTRY_PROPERTY_HANDLER_INDEX].property_name, DBUS_TYPE_UINT32, &de->index);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, entry_property_handlers[ENTRY_PROPERTY_HANDLER_NAME].property_name, DBUS_TYPE_STRING, &de->entry_name);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter, entry_property_handlers[ENTRY_PROPERTY_HANDLER_DEVICE].property_name, DBUS_TYPE_STRING, &device);

    pa_assert_se(dbus_message_iter_open_container(&dict_iter, DBUS_TYPE_DICT_ENTRY, NULL, &dict_entry_iter));

    pa_assert_se(dbus_message_iter_append_basic(&dict_entry_iter, DBUS_TYPE_STRING, &entry_property_handlers[ENTRY_PROPERTY_HANDLER_VOLUME].property_name));
    append_volume_variant(&dict_entry_iter, e);

    pa_assert_se(dbus_message_iter_close_container(&dict_iter, &dict_entry_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter, entry_property_handlers[ENTRY_PROPERTY_HANDLER_MUTE].property_name, DBUS_TYPE_BOOLEAN, &mute);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));

    pa_assert_se(dbus_connection_send(conn, reply, NULL));

    dbus_message_unref(reply);

    pa_xfree(e);
}

static void handle_entry_remove(DBusConnection *conn, DBusMessage *msg, void *userdata) {
    struct dbus_entry *de = userdata;
    pa_datum key;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(de);

    key.data = de->entry_name;
    key.size = strlen(de->entry_name);

    pa_assert_se(pa_database_unset(de->userdata->database, &key) == 0);

    send_entry_removed_signal(de);
    trigger_save(de->userdata);

    pa_assert_se(pa_hashmap_remove(de->userdata->dbus_entries, de->entry_name));
    dbus_entry_free(de);

    pa_dbus_send_empty_reply(conn, msg);
}

static char* _route_key(const char *name, const char *route) {
    pa_assert(name);
    pa_assert(route);

    return pa_sprintf_malloc("%s:%s", name, route);
}

static void _free_route_volumes(struct userdata *u) {
    struct route_volume *r;

    pa_assert(u);

    while ((r = u->route_volumes)) {
        PA_LLIST_REMOVE(struct route_volume, u->route_volumes, r);
        pa_xfree(r->name);
        pa_xfree(r);
    }
}

static struct route_entry* read_route_entry(struct userdata *u, const char *name, const char *route) {
    pa_datum key, data;
    char *route_key;
    struct route_entry *e;

    pa_assert(u);
    pa_assert(name);
    pa_assert(route);

    route_key = _route_key(name, route);
    key.data = (char*) route_key;
    key.size = strlen(route_key);

    pa_zero(data);

    pa_database_get(u->route_database, &key, &data);

    if (!data.data)
        goto fail;

    if (data.size != sizeof(struct route_entry)) {
        /* This is probably just a database upgrade, hence let's not
         * consider this more than a debug message */
        pa_log_debug("Database contains entry for route %s of wrong size %lu != %lu. Probably due to uprade, ignoring.", route, (unsigned long) data.size, (unsigned long) sizeof(struct route_entry));
        goto fail;
    }

    e = (struct route_entry*) data.data;

    if (e->version != ROUTE_ENTRY_VERSION) {
        pa_log_debug("Version of database entry for route %s doesn't match our version. Probably due to upgrade, ignoring.", route);
        goto fail;
    }

    if (!pa_cvolume_valid(&e->volume)) {
        pa_log_warn("Invalid volume stored in database for route %s :: %s", name, route);
        goto fail;
    }

    pa_xfree(route_key);

    return e;

fail:

    pa_xfree(route_key);
    pa_xfree(data.data);
    return NULL;
}

static pa_bool_t entry_has_volume_changed(struct entry *a, struct entry *b) {
    pa_assert(a);
    pa_assert(b);

    if (a->volume_valid)
        return !b->volume_valid ||
               !pa_channel_map_equal(&a->channel_map, &b->channel_map) ||
               !pa_cvolume_equal(&a->volume, &b->volume);
    else
        return b->volume_valid;
}

/* Iterate through all route volumes and apply their value to corresponding streams
 * in s-r database */
static void _apply_route_volumes(struct userdata *u, pa_bool_t apply) {

    struct entry *old;
    pa_datum key, data;
    struct route_volume *r;

    pa_assert(u);

    for (r = u->route_volumes; r; r = r->next) {
        struct entry entry;
        struct dbus_entry *de = NULL;

        if ((old = read_entry(u, r->name)))
            entry = *old;
        else
            /* If there is a route volume specified for a non-existent restore
             * entry, the route volume is ignored. */
            continue;

        entry.version = ENTRY_VERSION;
        pa_channel_map_init_mono(&entry.channel_map);
        pa_cvolume_set(&entry.volume, 1, r->volume.values[0]);
        entry.volume_valid = TRUE;

        if (!entry_has_volume_changed(old, &entry)) {
            pa_xfree(old);
            continue;
        }
        pa_xfree(old);

        key.data = (void*) r->name;
        key.size = (int)strlen(r->name);

        data.data = (void*) &entry;
        data.size = sizeof(entry);

        pa_log_info("Updating route %s volume/mute/device for stream %s.", u->route, r->name);
        pa_database_set(u->database, &key, &data, TRUE);

        pa_assert_se(de = pa_hashmap_get(u->dbus_entries, r->name));
        send_volume_updated_signal(de, &entry);

        if (apply)
            apply_entry(u, r->name, &entry);
    }
}

static void update_volumes(struct userdata *u) {
    struct route_entry* e;
    struct route_volume *r;
    char t[256];

    pa_assert(u);

    if ((u->use_sink_volume = have_sink_volume(u, u->route)) != NULL) {
        pa_log_debug("Using sink-volume for mode %s.", u->route);

        if (u->subscription) {
            pa_subscription_free(u->subscription);
            u->subscription = NULL;
        }
        if (!u->sink_subscription)
            u->sink_subscription = pa_subscription_new(u->core,
                                                       PA_SUBSCRIPTION_MASK_SINK,
                                                       sink_volume_subscribe_cb,
                                                       u);

        if (u->route_volumes) {
            r = u->route_volumes;
            e = read_route_entry(u, r->name, u->route);
            if (e) {
                r->volume = e->volume;
                pa_xfree(e);
            } else {
                r->volume = r->default_volume;
            }
            set_route_volumes(u, &r->volume);
            _set_streams(u, PA_VOLUME_NORM, -1);
            pa_log_debug("Restoring volume to sink %s: %s", u->use_sink_volume->sink->name,
                                                            pa_cvolume_snprint(t, sizeof(t), &r->volume));
            _sink_set_volume(u->use_sink_volume->sink, &r->volume);
            _apply_route_volumes(u, FALSE);
            trigger_save(u);

            proxy_volume_all(u);
        }

        return;

    } else {
        if (u->sink_subscription) {
            pa_subscription_free(u->sink_subscription);
            u->sink_subscription = NULL;
        }
        if (!u->subscription)
            u->subscription = pa_subscription_new(u->core,
                                                  PA_SUBSCRIPTION_MASK_SINK_INPUT|PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT,
                                                  subscribe_callback,
                                                  u);
    }

    /* ideally, scale stream-restore rules, by d dB: this is not
       applicable with pa_cvolume (lose precision around 0U), and
       would not be user friendly, because we don't have "sink" volume
       bar: in some situation, a "role" volume would not be restored
       because some other "role" would have constant higher volume. */

    /* instead, let's restore our configured streams */

    for (r = u->route_volumes; r; r = r->next) {
        e = read_route_entry(u, r->name, u->route);

        if (!e) {
            r->volume = r->default_volume;
        } else {

            if (!pa_cvolume_valid(&e->volume)) {
                r->volume = r->default_volume;
            } else {

                if (r->reset_min_volume && e->volume.values[0] < r->min_volume.values[0])
                    r->volume = r->default_volume;
                else
                    r->volume = e->volume;
            }
            pa_xfree(e);
        }
        _proxy_volume(u, r->name, &r->volume);

        pa_log_debug("Restored stream %s route %s volume=%s", r->name, u->route, pa_cvolume_snprint(t, sizeof(t), &r->volume));
    }

    _apply_route_volumes(u, TRUE);
    trigger_save(u);

    return;
}

static void check_mode(pa_sink *s, struct userdata *u) {
    const char *mode;

    pa_assert(s);
    pa_assert(u);

    mode = pa_proplist_gets(s->proplist, PA_NOKIA_PROP_AUDIO_MODE);

    if (mode) {
        if (u->route && pa_streq(mode, u->route))
            return;

        if (u->route)
            pa_xfree(u->route);

        u->route = pa_xstrdup(mode);

        update_volumes(u);
    }
}

static pa_hook_result_t sink_proplist_changed_hook_callback(pa_core *c, pa_sink *s, struct userdata *u) {
    pa_sink_input *i;
    uint32_t idx;
    const char *name;

    pa_assert(s);
    pa_assert(u);

    PA_IDXSET_FOREACH(i, s->inputs, idx) {
        name = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_NAME);
        if (name && pa_streq(name, VOICE_MASTER_SINK_INPUT_NAME)) {
            check_mode(s, u);
            break;
        }
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t hw_sink_input_move_finish_callback(pa_core *c, pa_sink_input *i, struct userdata *u) {
    const char *name;

    pa_assert(i);
    pa_assert(u);

    name = pa_proplist_gets(i->proplist, PA_PROP_MEDIA_NAME);

    if (i->sink && name && pa_streq(name, VOICE_MASTER_SINK_INPUT_NAME))
        check_mode(i->sink, u);

    return PA_HOOK_OK;
}

static void save_time_callback(pa_mainloop_api*a, pa_time_event* e, const struct timeval *t, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(a);
    pa_assert(e);
    pa_assert(u);

    pa_assert(e == u->save_time_event);
    u->core->mainloop->time_free(u->save_time_event);
    u->save_time_event = NULL;

    pa_database_sync(u->database);
    pa_database_sync(u->route_database);

    pa_log_info("Synced.");
}

static char *get_name(pa_proplist *p, const char *prefix) {
    const char *r;
    char *t;

    if (!p)
        return NULL;

    if ((r = pa_proplist_gets(p, IDENTIFICATION_PROPERTY)))
        return pa_xstrdup(r);

    if ((r = pa_proplist_gets(p, PA_PROP_MEDIA_ROLE)))
        t = pa_sprintf_malloc("%s-by-media-role:%s", prefix, r);
    else if ((r = pa_proplist_gets(p, PA_PROP_APPLICATION_ID)))
        t = pa_sprintf_malloc("%s-by-application-id:%s", prefix, r);
    else if ((r = pa_proplist_gets(p, PA_PROP_APPLICATION_NAME)))
        t = pa_sprintf_malloc("%s-by-application-name:%s", prefix, r);
    else if ((r = pa_proplist_gets(p, PA_PROP_MEDIA_NAME)))
        t = pa_sprintf_malloc("%s-by-media-name:%s", prefix, r);
    else
        t = pa_sprintf_malloc("%s-fallback:%s", prefix, r);

    pa_proplist_sets(p, IDENTIFICATION_PROPERTY, t);
    return t;
}

static struct entry *read_entry(struct userdata *u, const char *name) {
    pa_datum key, data;
    struct entry *e;

    pa_assert(u);
    pa_assert(name);

    key.data = (char*) name;
    key.size = strlen(name);

    pa_zero(data);

    if (!pa_database_get(u->database, &key, &data))
        goto fail;

    if (data.size != sizeof(struct entry)) {
        /* This is probably just a database upgrade, hence let's not
         * consider this more than a debug message */
        pa_log_debug("Database contains entry for stream %s of wrong size %lu != %lu. Probably due to uprade, ignoring.", name, (unsigned long) data.size, (unsigned long) sizeof(struct entry));
        goto fail;
    }

    e = (struct entry*) data.data;

    if (e->version != ENTRY_VERSION) {
        pa_log_debug("Version of database entry for stream %s doesn't match our version. Probably due to upgrade, ignoring.", name);
        goto fail;
    }

    if (!memchr(e->device, 0, sizeof(e->device))) {
        pa_log_warn("Database contains entry for stream %s with missing NUL byte in device name", name);
        goto fail;
    }

    if (!memchr(e->card, 0, sizeof(e->card))) {
        pa_log_warn("Database contains entry for stream %s with missing NUL byte in card name", name);
        goto fail;
    }

    if (e->device_valid && !pa_namereg_is_valid_name(e->device)) {
        pa_log_warn("Invalid device name stored in database for stream %s", name);
        goto fail;
    }

    if (e->card_valid && !pa_namereg_is_valid_name(e->card)) {
        pa_log_warn("Invalid card name stored in database for stream %s", name);
        goto fail;
    }

    if (e->volume_valid && !pa_channel_map_valid(&e->channel_map)) {
        pa_log_warn("Invalid channel map stored in database for stream %s", name);
        goto fail;
    }

    if (e->volume_valid && (!pa_cvolume_valid(&e->volume) || !pa_cvolume_compatible_with_channel_map(&e->volume, &e->channel_map))) {
        pa_log_warn("Invalid volume stored in database for stream %s", name);
        goto fail;
    }

    return e;

fail:

    pa_datum_free(&data);
    return NULL;
}

static void trigger_save(struct userdata *u) {
    pa_native_connection *c;
    uint32_t idx;
    struct route_volume *r;

    for (c = pa_idxset_first(u->subscribed, &idx); c; c = pa_idxset_next(u->subscribed, &idx)) {
        pa_tagstruct *t;

        t = pa_tagstruct_new(NULL, 0);
        pa_tagstruct_putu32(t, PA_COMMAND_EXTENSION);
        pa_tagstruct_putu32(t, 0);
        pa_tagstruct_putu32(t, u->module->index);
        pa_tagstruct_puts(t, u->module->name);
        pa_tagstruct_putu32(t, SUBCOMMAND_EVENT);

        pa_pstream_send_tagstruct(pa_native_connection_get_pstream(c), t);
    }

    if (!u->restore_route_volume)
        goto end;

    if (!u->route)
        goto end;

    for (r = u->route_volumes; r; r = r->next) {
        struct route_entry entry;
        pa_datum key, data;
        char *route_key;
        char t[256];

        if (!pa_cvolume_valid(&r->volume))
            continue;

        route_key = _route_key(r->name, u->route);

        memset(&entry, 0, sizeof(entry));
        entry.version = ROUTE_ENTRY_VERSION;
        entry.volume = r->volume;

        key.data = (void*) route_key;
        key.size = (int) strlen(route_key);

        data.data = (void*) &entry;
        data.size = sizeof(entry);

        pa_database_set(u->route_database, &key, &data, TRUE);

        pa_log_debug("Save stream %s route %s volume=%s", u->route, r->name, pa_cvolume_snprint(t, sizeof(t), &r->volume));

        pa_xfree(route_key);
    }

end:
    if (u->save_time_event)
        return;

    u->save_time_event = pa_core_rttime_new(u->core, pa_rtclock_now() + SAVE_INTERVAL, save_time_callback, u);
}

static pa_bool_t entries_equal(const struct entry *a, const struct entry *b) {

    pa_assert(a);
    pa_assert(b);

    if (a->device_valid != b->device_valid ||
        (a->device_valid && strncmp(a->device, b->device, sizeof(a->device))))
        return FALSE;

    if (a->card_valid != b->card_valid ||
        (a->card_valid && strncmp(a->card, b->card, sizeof(a->card))))
        return FALSE;

    if (a->muted_valid != b->muted_valid ||
        (a->muted_valid && (a->muted != b->muted)))
        return FALSE;

    if (a->volume_valid != b->volume_valid)
        return FALSE;

    if (a->volume_valid && !pa_cvolume_equal(&a->volume, &b->volume))
        return FALSE;

    return TRUE;
}

static void subscribe_callback(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    struct userdata *u = userdata;
    struct entry entry, *old = NULL;
    char *name = NULL;
    pa_datum key, data;
    pa_bool_t created_new_entry = TRUE;
    pa_bool_t device_updated = FALSE;
    pa_bool_t volume_updated = FALSE;
    pa_bool_t mute_updated = FALSE;
    struct dbus_entry *de = NULL;

    pa_assert(c);
    pa_assert(u);

    if (t != (PA_SUBSCRIPTION_EVENT_SINK_INPUT|PA_SUBSCRIPTION_EVENT_NEW) &&
        t != (PA_SUBSCRIPTION_EVENT_SINK_INPUT|PA_SUBSCRIPTION_EVENT_CHANGE) &&
        t != (PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT|PA_SUBSCRIPTION_EVENT_NEW) &&
        t != (PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT|PA_SUBSCRIPTION_EVENT_CHANGE))
        return;

    pa_zero(entry);
    entry.version = ENTRY_VERSION;

    if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK_INPUT) {
        pa_sink_input *sink_input;

        if (!(sink_input = pa_idxset_get_by_index(c->sink_inputs, idx)))
            return;

        if (!(name = get_name(sink_input->proplist, "sink-input")))
            return;

        if ((old = read_entry(u, name))) {
            entry = *old;
            created_new_entry = FALSE;
        }

        if (sink_input->save_volume) {
            pa_assert(sink_input->volume_writable);

            entry.channel_map = sink_input->channel_map;
            pa_sink_input_get_volume(sink_input, &entry.volume, FALSE);
            entry.volume_valid = TRUE;

            volume_updated = !created_new_entry
                             && (!old->volume_valid
                                 || !pa_channel_map_equal(&entry.channel_map, &old->channel_map)
                                 || !pa_cvolume_equal(&entry.volume, &old->volume));
        }

        if (sink_input->save_muted) {
            entry.muted = pa_sink_input_get_mute(sink_input);
            entry.muted_valid = TRUE;

            mute_updated = !created_new_entry && (!old->muted_valid || entry.muted != old->muted);
        }

        if (sink_input->save_sink) {
            pa_strlcpy(entry.device, sink_input->sink->name, sizeof(entry.device));
            entry.device_valid = TRUE;

            device_updated = !created_new_entry && (!old->device_valid || !pa_streq(entry.device, old->device));
            if (sink_input->sink->card) {
                pa_strlcpy(entry.card, sink_input->sink->card->name, sizeof(entry.card));
                entry.card_valid = TRUE;
            }
        }

    } else {
        pa_source_output *source_output;

        pa_assert((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT);

        if (!(source_output = pa_idxset_get_by_index(c->source_outputs, idx)))
            return;

        if (!(name = get_name(source_output->proplist, "source-output")))
            return;

        if ((old = read_entry(u, name))) {
            entry = *old;
            created_new_entry = FALSE;
        }

        if (source_output->save_source) {
            pa_strlcpy(entry.device, source_output->source->name, sizeof(entry.device));
            entry.device_valid = source_output->save_source;

            device_updated = !created_new_entry && (!old->device_valid || !pa_streq(entry.device, old->device));

            if (source_output->source->card) {
                pa_strlcpy(entry.card, source_output->source->card->name, sizeof(entry.card));
                entry.card_valid = TRUE;
            }
        }
    }

    if (old) {

        if (entries_equal(old, &entry)) {
            pa_xfree(old);
            pa_xfree(name);
            return;
        }

        pa_xfree(old);
    }

    if (entry.volume_valid) {
        set_route_volume_by_name(u, name, &entry.volume);
    }

    key.data = name;
    key.size = strlen(name);

    data.data = &entry;
    data.size = sizeof(entry);

    pa_log_info("Storing volume/mute/device for stream %s.", name);

    pa_database_set(u->database, &key, &data, TRUE);

    if (created_new_entry) {
        de = dbus_entry_new(u, name);
        pa_assert_se(pa_hashmap_put(u->dbus_entries, de->entry_name, de) == 0);
        send_new_entry_signal(de);
    } else {
        pa_assert_se(de = pa_hashmap_get(u->dbus_entries, name));

        if (device_updated)
            send_device_updated_signal(de, &entry);
        if (volume_updated)
            send_volume_updated_signal(de, &entry);
        if (mute_updated)
            send_mute_updated_signal(de, &entry);
    }

    if (entry.volume_valid)
        _proxy_volume(u, name, &entry.volume);

    pa_xfree(name);

    trigger_save(u);
}

static void sink_volume_subscribe_cb(pa_core *c, pa_subscription_event_type_t t, uint32_t idx, void *userdata) {
    struct userdata *u = userdata;
    pa_sink *sink;
    const pa_cvolume *vol;

    pa_assert(c);
    pa_assert(u);

    if (t != (PA_SUBSCRIPTION_EVENT_SINK|PA_SUBSCRIPTION_EVENT_CHANGE))
        return;

    if (!u->use_sink_volume)
        return;

    pa_assert(u->use_sink_volume->sink);

    if (!(sink = pa_idxset_get_by_index(c->sinks, idx)))
        return;

    if (u->use_sink_volume->sink != sink)
        return;

    vol = pa_sink_get_volume(sink, FALSE);

    if (pa_cvolume_valid(vol)) {
        pa_log_debug("sink_volume_subscribe_cb() sink volume changes to %u", pa_cvolume_max(vol));

        /* set all route volumes to sink volume */
        set_route_volumes(u, vol);
        _apply_route_volumes(u, FALSE);
        trigger_save(u);

        /* proxy all route volumes, since we have only one volume in sink-volume mode */
        proxy_volume_all(u);
    }
}

static pa_hook_result_t sink_input_new_hook_callback(pa_core *c, pa_sink_input_new_data *new_data, struct userdata *u) {
    char *name;
    struct entry *e;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);
    pa_assert(u->restore_device);

    if (!(name = get_name(new_data->proplist, "sink-input")))
        return PA_HOOK_OK;

    if (new_data->sink)
        pa_log_debug("Not restoring device for stream %s, because already set.", name);
    else if ((e = read_entry(u, name))) {
        pa_sink *s = NULL;

        if (e->device_valid)
            s = pa_namereg_get(c, e->device, PA_NAMEREG_SINK);

        if (!s && e->card_valid) {
            pa_card *card;

            if ((card = pa_namereg_get(c, e->card, PA_NAMEREG_CARD)))
                s = pa_idxset_first(card->sinks, NULL);
        }

        /* It might happen that a stream and a sink are set up at the
           same time, in which case we want to make sure we don't
           interfere with that */
        if (s && PA_SINK_IS_LINKED(pa_sink_get_state(s))) {
            pa_log_info("Restoring device for stream %s.", name);
            new_data->sink = s;
            new_data->save_sink = TRUE;
        }

        pa_xfree(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_input_fixate_hook_callback(pa_core *c, pa_sink_input_new_data *new_data, struct userdata *u) {
    char *name;
    struct entry *e;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);
    pa_assert(u->restore_volume || u->restore_muted);

    if (!(name = get_name(new_data->proplist, "sink-input")))
        return PA_HOOK_OK;

    if ((e = read_entry(u, name))) {

        if (u->restore_volume && e->volume_valid) {
            if (!new_data->volume_writable)
                pa_log_debug("Not restoring volume for sink input %s, because its volume is read-only.", name);
            else if (new_data->volume_is_set)
                pa_log_debug("Not restoring volume for sink input %s, because already set.", name);
            else {
                char buf[PA_CVOLUME_SNPRINT_MAX];

                /* If we are in sink-volume mode and our route role streams appear, we set them to
                 * PA_VOLUME_NORM */
                if (u->use_sink_volume && (get_route_volume_by_name(u, name) != NULL))
                    pa_cvolume_set(&e->volume, e->volume.channels, PA_VOLUME_NORM);

                pa_log_info("Restoring volume for sink input %s.", name);
                pa_sink_input_new_data_set_volume(new_data, pa_cvolume_remap(&e->volume, &e->channel_map, &new_data->channel_map));
                pa_log_info("Restored volume: %s", pa_cvolume_snprint(buf, PA_CVOLUME_SNPRINT_MAX, &new_data->volume));
            }
        }

        if (u->restore_muted && e->muted_valid) {

            if (!new_data->muted_is_set) {
                pa_log_info("Restoring mute state for sink input %s.", name);
                pa_sink_input_new_data_set_muted(new_data, e->muted);
                new_data->save_muted = TRUE;
            } else
                pa_log_debug("Not restoring mute state for sink input %s, because already set.", name);
        }

        pa_xfree(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t source_output_new_hook_callback(pa_core *c, pa_source_output_new_data *new_data, struct userdata *u) {
    char *name;
    struct entry *e;

    pa_assert(c);
    pa_assert(new_data);
    pa_assert(u);
    pa_assert(u->restore_device);

    if (new_data->direct_on_input)
        return PA_HOOK_OK;

    if (!(name = get_name(new_data->proplist, "source-output")))
        return PA_HOOK_OK;

    if (new_data->source)
        pa_log_debug("Not restoring device for stream %s, because already set", name);
    else if ((e = read_entry(u, name))) {
        pa_source *s = NULL;

        if (e->device_valid)
            s = pa_namereg_get(c, e->device, PA_NAMEREG_SOURCE);

        if (!s && e->card_valid) {
            pa_card *card;

            if ((card = pa_namereg_get(c, e->card, PA_NAMEREG_CARD)))
                s = pa_idxset_first(card->sources, NULL);
        }

        /* It might happen that a stream and a sink are set up at the
           same time, in which case we want to make sure we don't
           interfere with that */
        if (s && PA_SOURCE_IS_LINKED(pa_source_get_state(s))) {
            pa_log_info("Restoring device for stream %s.", name);
            new_data->source = s;
            new_data->save_source = TRUE;
        }

        pa_xfree(e);
    }

    pa_xfree(name);

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_put_hook_callback(pa_core *c, pa_sink *sink, struct userdata *u) {
    pa_sink_input *si;
    uint32_t idx;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    pa_assert(u->on_hotplug && u->restore_device);

    PA_IDXSET_FOREACH(si, c->sink_inputs, idx) {
        char *name;
        struct entry *e;

        if (si->sink == sink)
            continue;

        if (si->save_sink)
            continue;

        /* Skip this if it is already in the process of being moved
         * anyway */
        if (!si->sink)
            continue;

        /* It might happen that a stream and a sink are set up at the
           same time, in which case we want to make sure we don't
           interfere with that */
        if (!PA_SINK_INPUT_IS_LINKED(pa_sink_input_get_state(si)))
            continue;

        if (!(name = get_name(si->proplist, "sink-input")))
            continue;

        if ((e = read_entry(u, name))) {
            if (e->device_valid && pa_streq(e->device, sink->name))
                pa_sink_input_move_to(si, sink, TRUE);

            pa_xfree(e);
        }

        pa_xfree(name);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t source_put_hook_callback(pa_core *c, pa_source *source, struct userdata *u) {
    pa_source_output *so;
    uint32_t idx;

    pa_assert(c);
    pa_assert(source);
    pa_assert(u);
    pa_assert(u->on_hotplug && u->restore_device);

    PA_IDXSET_FOREACH(so, c->source_outputs, idx) {
        char *name;
        struct entry *e;

        if (so->source == source)
            continue;

        if (so->save_source)
            continue;

        if (so->direct_on_input)
            continue;

        /* Skip this if it is already in the process of being moved anyway */
        if (!so->source)
            continue;

        /* It might happen that a stream and a source are set up at the
           same time, in which case we want to make sure we don't
           interfere with that */
        if (!PA_SOURCE_OUTPUT_IS_LINKED(pa_source_output_get_state(so)))
            continue;

        if (!(name = get_name(so->proplist, "source-output")))
            continue;

        if ((e = read_entry(u, name))) {
            if (e->device_valid && pa_streq(e->device, source->name))
                pa_source_output_move_to(so, source, TRUE);

            pa_xfree(e);
        }

        pa_xfree(name);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t sink_unlink_hook_callback(pa_core *c, pa_sink *sink, struct userdata *u) {
    pa_sink_input *si;
    uint32_t idx;

    pa_assert(c);
    pa_assert(sink);
    pa_assert(u);
    pa_assert(u->on_rescue && u->restore_device);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    PA_IDXSET_FOREACH(si, sink->inputs, idx) {
        char *name;
        struct entry *e;

        if (!si->sink)
            continue;

        if (!(name = get_name(si->proplist, "sink-input")))
            continue;

        if ((e = read_entry(u, name))) {

            if (e->device_valid) {
                pa_sink *d;

                if ((d = pa_namereg_get(c, e->device, PA_NAMEREG_SINK)) &&
                    d != sink &&
                    PA_SINK_IS_LINKED(pa_sink_get_state(d)))
                    pa_sink_input_move_to(si, d, TRUE);
            }

            pa_xfree(e);
        }

        pa_xfree(name);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t source_unlink_hook_callback(pa_core *c, pa_source *source, struct userdata *u) {
    pa_source_output *so;
    uint32_t idx;

    pa_assert(c);
    pa_assert(source);
    pa_assert(u);
    pa_assert(u->on_rescue && u->restore_device);

    /* There's no point in doing anything if the core is shut down anyway */
    if (c->state == PA_CORE_SHUTDOWN)
        return PA_HOOK_OK;

    PA_IDXSET_FOREACH(so, source->outputs, idx) {
        char *name;
        struct entry *e;

        if (so->direct_on_input)
            continue;

        if (!so->source)
            continue;

        if (!(name = get_name(so->proplist, "source-output")))
            continue;

        if ((e = read_entry(u, name))) {

            if (e->device_valid) {
                pa_source *d;

                if ((d = pa_namereg_get(c, e->device, PA_NAMEREG_SOURCE)) &&
                    d != source &&
                    PA_SOURCE_IS_LINKED(pa_source_get_state(d)))
                    pa_source_output_move_to(so, d, TRUE);
            }

            pa_xfree(e);
        }

        pa_xfree(name);
    }

    return PA_HOOK_OK;
}

static int fill_db(struct userdata *u, const char *filename) {
    FILE *f;
    int n = 0;
    int ret = -1;
    char *fn = NULL;

    pa_assert(u);

    if (filename)
        f = fopen(fn = pa_xstrdup(filename), "r");
    else
        f = pa_open_config_file(DEFAULT_FALLBACK_FILE, DEFAULT_FALLBACK_FILE_USER, NULL, &fn);

    if (!f) {
        pa_log("Failed to open file config file: %s", pa_cstrerror(errno));
        goto finish;
    }

    pa_lock_fd(fileno(f), 1);

    while (!feof(f)) {
        char ln[256];
        char *d, *v;
        double db;

        if (!fgets(ln, sizeof(ln), f))
            break;

        n++;

        pa_strip_nl(ln);

        if (ln[0] == '#' || !*ln )
            continue;

        d = ln+strcspn(ln, WHITESPACE);
        v = d+strspn(d, WHITESPACE);
        d[0] = '\0';

        if (!*v) {
            pa_log(__FILE__ ": [%s:%u] failed to parse line - too few words", filename, n);
            goto finish;
        }

        *d = 0;
        if (pa_atod(v, &db) >= 0) {
            pa_datum key, data;
            struct entry e;

            memset(&e, 0, sizeof(e));
            e.version = ENTRY_VERSION;
            e.volume_valid = TRUE;
            pa_cvolume_set(&e.volume, 1, pa_sw_volume_from_dB(db));
            pa_channel_map_init_mono(&e.channel_map);

            key.data = (void*) ln;
            key.size = strlen(ln);

            data.data = (void*) &e;
            data.size = sizeof(e);

            /* save default volume only if it doesn't exist in database yet. */
            if (pa_database_set(u->database, &key, &data, FALSE) == 0)
                pa_log_debug("Setting %s to %fdb", ln, db);
        }
    }

    trigger_save(u);
    ret = 0;

finish:
    if (f) {
        pa_lock_fd(fileno(f), 0);
        fclose(f);
    }

    if (fn)
        pa_xfree(fn);

    return ret;
}

static int fill_route_db(struct userdata *u, const char *filename) {
    FILE *f;
    int n = 0;
    int ret = -1;
    char *fn = NULL;

    pa_assert(u);

    if (filename)
        f = fopen(fn = pa_xstrdup(filename), "r");
    else
        f = pa_open_config_file(DEFAULT_ROUTE_FILE, DEFAULT_ROUTE_FILE_USER, NULL, &fn);

    if (!f) {
        pa_log("Failed to open file config file: %s", pa_cstrerror(errno));
        goto finish;
    }

    pa_lock_fd(fileno(f), 1);

    while (!feof(f)) {
        char ln[256];
        char *d, *v, *min;
        double db;

        if (!fgets(ln, sizeof(ln), f))
            break;

        n++;

        pa_strip_nl(ln);

        if (ln[0] == '#' || !*ln )
            continue;

        d = ln+strcspn(ln, WHITESPACE);
        v = d+strspn(d, WHITESPACE);
        d[0] = '\0';
        d = v+strcspn(v, WHITESPACE);
        min = d+strspn(d, WHITESPACE);
        d[0] = '\0';

        if (!*v) {
            pa_log(__FILE__ ": [%s:%u] failed to parse line - too few words", filename, n);
            goto finish;
        }

        *d = 0;
        if (pa_atod(v, &db) >= 0) {
            struct route_volume *r;

            r = pa_xnew0(struct route_volume, 1);
            PA_LLIST_INIT(struct route_volume, r);

            r->name = pa_xstrdup(ln);
            pa_cvolume_set(&r->volume, 1, pa_sw_volume_from_dB(db));
            r->default_volume = r->volume;

            pa_log_debug("Adding route with stream name %s\n", r->name);
            if (*min) {
                if (pa_atod(min, &db) >= 0) {
                    pa_cvolume_set(&r->min_volume, 1, pa_sw_volume_from_dB(db));
                    r->reset_min_volume = TRUE;
                    pa_log_debug("Setting %s minimum value to %fdB", ln, db);
                }
            }

            /* append new route volume entry to list */
            PA_LLIST_PREPEND(struct route_volume, u->route_volumes, r);
        }
    }

    ret = 0;

finish:
    if (f) {
        pa_lock_fd(fileno(f), 0);
        fclose(f);
    }

    if (fn)
        pa_xfree(fn);

    return ret;
}

static int fill_sink_db(struct userdata *u, const char *filename) {
    FILE *f;
    int n = 0;
    int ret = -1;
    char *fn = NULL;

    pa_assert(u);

    if (filename)
        f = fopen(fn = pa_xstrdup(filename), "r");
    else
        f = pa_open_config_file(DEFAULT_SINK_VOLUME_FILE, DEFAULT_SINK_VOLUME_FILE_USER, NULL, &fn);

    if (!f) {
        pa_log_warn("Failed to open sink-volume-table file: %s", pa_cstrerror(errno));
        goto finish;
    }

    pa_lock_fd(fileno(f), 1);

    while (!feof(f)) {
        char ln[256];
        char *d, *mode, *sink_name;
        struct sink_volume *v;

        n++;

        if (!fgets(ln, sizeof(ln), f))
            break;

        pa_strip_nl(ln);

        if (ln[0] == '#' || !*ln)
            continue;

        mode = ln + strspn(ln, WHITESPACE);
        d = mode + strcspn(mode, ":");
        d[0] = '\0';
        sink_name = d + 1;

        if (!*sink_name) {
            pa_log_error("[%s:%u] failed to parse line", filename, n);
            goto finish;
        }

        v = pa_xnew0(struct sink_volume, 1);
        PA_LLIST_INIT(struct sink_volume, v);

        v->mode = pa_xstrdup(mode);
        v->sink_name = pa_xstrdup(sink_name);
        v->sink = pa_namereg_get(u->core, sink_name, PA_NAMEREG_SINK);

        PA_LLIST_PREPEND(struct sink_volume, u->sink_volumes, v);

        pa_log_debug("sink-volume, mode \"%s\" controls sink \"%s\"", mode, sink_name);
    }

    ret = 0;

finish:
    if (f) {
        pa_lock_fd(fileno(f), 0);
        fclose(f);
    }

    if (fn)
        pa_xfree(fn);

    return ret;
}

static void apply_entry(struct userdata *u, const char *name, struct entry *e) {
    pa_sink_input *si;
    pa_source_output *so;
    uint32_t idx;

    pa_assert(u);
    pa_assert(name);
    pa_assert(e);

    PA_IDXSET_FOREACH(si, u->core->sink_inputs, idx) {
        char *n;
        pa_sink *s;

        if (!si->sink) /* for eg. moving */
            continue;

        if (!(n = get_name(si->proplist, "sink-input")))
            continue;

        if (!pa_streq(name, n)) {
            pa_xfree(n);
            continue;
        }
        pa_xfree(n);

        if (u->restore_volume && e->volume_valid && si->volume_writable) {
            pa_cvolume v;

            pa_log_info("Restoring volume for sink input %s. c %d vol %d", name, e->channel_map.channels, e->volume.values[0]);
            v = e->volume;
            pa_sink_input_set_volume(si, pa_cvolume_remap(&v, &e->channel_map, &si->channel_map), TRUE, FALSE);
        }

        if (u->restore_muted && e->muted_valid) {
            pa_log_info("Restoring mute state for sink input %s.", name);
            pa_sink_input_set_mute(si, e->muted, TRUE);
        }

        if (u->restore_device &&
            e->device_valid &&
            (s = pa_namereg_get(u->core, e->device, PA_NAMEREG_SINK))) {

            pa_log_info("Restoring device for stream %s.", name);
            pa_sink_input_move_to(si, s, TRUE);
        }
    }

    PA_IDXSET_FOREACH(so, u->core->source_outputs, idx) {
        char *n;
        pa_source *s;

        if (!so->source) /* for eg. moving */
            continue;

        if (!(n = get_name(so->proplist, "source-output")))
            continue;

        if (!pa_streq(name, n)) {
            pa_xfree(n);
            continue;
        }
        pa_xfree(n);

        if (u->restore_device &&
            e->device_valid &&
            (s = pa_namereg_get(u->core, e->device, PA_NAMEREG_SOURCE))) {

            pa_log_info("Restoring device for stream %s.", name);
            pa_source_output_move_to(so, s, TRUE);
        }
    }
}

#ifdef DEBUG_VOLUME
PA_GCC_UNUSED static void stream_restore_dump_database(struct userdata *u) {
    pa_datum key;
    pa_bool_t done;

    done = !pa_database_first(u->database, &key, NULL);

    while (!done) {
        pa_datum next_key;
        struct entry *e;
        char *name;

        done = !pa_database_next(u->database, &key, &next_key, NULL);

        name = pa_xstrndup(key.data, key.size);
        pa_datum_free(&key);

        if ((e = read_entry(u, name))) {
            char t[256];
            pa_log("name=%s", name);
            pa_log("device=%s %s", e->device, pa_yes_no(e->device_valid));
            pa_log("channel_map=%s", pa_channel_map_snprint(t, sizeof(t), &e->channel_map));
            pa_log("volume=%s %s", pa_cvolume_snprint(t, sizeof(t), &e->volume), pa_yes_no(e->volume_valid));
            pa_log("mute=%s %s", pa_yes_no(e->muted), pa_yes_no(e->volume_valid));
            pa_xfree(e);
        }

        pa_xfree(name);

        key = next_key;
    }
}
#endif

#define EXT_VERSION 2

static int extension_cb(pa_native_protocol *p, pa_module *m, pa_native_connection *c, uint32_t tag, pa_tagstruct *t) {
    struct userdata *u;
    uint32_t command;
    pa_tagstruct *reply = NULL;

    pa_assert(p);
    pa_assert(m);
    pa_assert(c);
    pa_assert(t);

    u = m->userdata;

    if (pa_tagstruct_getu32(t, &command) < 0)
        goto fail;

    reply = pa_tagstruct_new(NULL, 0);
    pa_tagstruct_putu32(reply, PA_COMMAND_REPLY);
    pa_tagstruct_putu32(reply, tag);

    switch (command) {
        case SUBCOMMAND_TEST: {
            if (!pa_tagstruct_eof(t))
                goto fail;

            pa_tagstruct_putu32(reply, EXT_VERSION);
            break;
        }

        case SUBCOMMAND_READ: {
            pa_datum key;
            pa_bool_t done;

            if (!pa_tagstruct_eof(t))
                goto fail;

            done = !pa_database_first(u->database, &key, NULL);

            while (!done) {
                pa_datum next_key;
                struct entry *e;
                char *name;

                done = !pa_database_next(u->database, &key, &next_key, NULL);

                name = pa_xstrndup(key.data, key.size);
                pa_datum_free(&key);

                if ((e = read_entry(u, name))) {
                    pa_cvolume r;
                    pa_channel_map cm;

                    pa_tagstruct_puts(reply, name);
                    pa_tagstruct_put_channel_map(reply, e->volume_valid ? &e->channel_map : pa_channel_map_init(&cm));
                    pa_tagstruct_put_cvolume(reply, e->volume_valid ? &e->volume : pa_cvolume_init(&r));
                    pa_tagstruct_puts(reply, e->device_valid ? e->device : NULL);
                    pa_tagstruct_put_boolean(reply, e->muted_valid ? e->muted : FALSE);

                    pa_xfree(e);
                }

                pa_xfree(name);

                key = next_key;
            }

            break;
        }

        case SUBCOMMAND_WRITE: {
            uint32_t mode;
            pa_bool_t apply_immediately = FALSE;

            if (pa_tagstruct_getu32(t, &mode) < 0 ||
                pa_tagstruct_get_boolean(t, &apply_immediately) < 0)
                goto fail;

            if (mode != PA_UPDATE_MERGE &&
                mode != PA_UPDATE_REPLACE &&
                mode != PA_UPDATE_SET)
                goto fail;

            if (mode == PA_UPDATE_SET) {
                struct dbus_entry *de;
                void *state = NULL;

                PA_HASHMAP_FOREACH(de, u->dbus_entries, state) {
                    send_entry_removed_signal(de);
                    dbus_entry_free(pa_hashmap_remove(u->dbus_entries, de->entry_name));
                }
                pa_database_clear(u->database);
            }

            while (!pa_tagstruct_eof(t)) {
                const char *name, *device;
                pa_bool_t muted;
                struct entry entry;
                pa_datum key, data;
                pa_cvolume r;
                struct entry *old;

                pa_zero(entry);
                entry.version = ENTRY_VERSION;

                if (pa_tagstruct_gets(t, &name) < 0 ||
                    pa_tagstruct_get_channel_map(t, &entry.channel_map) ||
                    pa_tagstruct_get_cvolume(t, &r) < 0 ||
                    pa_tagstruct_gets(t, &device) < 0 ||
                    pa_tagstruct_get_boolean(t, &muted) < 0)
                    goto fail;

                entry.volume = r;

                if (!name || !*name)
                    goto fail;

                entry.volume_valid = entry.volume.channels > 0;

                if (entry.volume_valid)
                    if (!pa_cvolume_compatible_with_channel_map(&r, &entry.channel_map))
                        goto fail;

                entry.muted = muted;
                entry.muted_valid = TRUE;

                if (device)
                    pa_strlcpy(entry.device, device, sizeof(entry.device));
                entry.device_valid = !!entry.device[0];

                if (entry.device_valid &&
                    !pa_namereg_is_valid_name(entry.device))
                    goto fail;

                old = read_entry(u, name);

                if (entry.volume_valid) {
                    if (u->use_sink_volume)
                        set_route_volumes(u, &entry.volume);
                    else {
                        set_route_volume_by_name(u, name, &entry.volume);
                        _proxy_volume(u, name, &entry.volume);
                    }
                }

                key.data = (char*) name;
                key.size = strlen(name);

                data.data = &entry;
                data.size = sizeof(entry);

                pa_log_debug("Client %s changes entry %s.",
                             pa_strnull(pa_proplist_gets(pa_native_connection_get_client(c)->proplist, PA_PROP_APPLICATION_PROCESS_BINARY)),
                             name);

                if (pa_database_set(u->database, &key, &data, mode == PA_UPDATE_REPLACE) == 0) {
                    struct dbus_entry *de;

                    if (old) {
                        pa_assert_se((de = pa_hashmap_get(u->dbus_entries, name)));

                        if ((old->device_valid != entry.device_valid)
                            || (entry.device_valid && !pa_streq(entry.device, old->device)))
                            send_device_updated_signal(de, &entry);

                        if ((old->volume_valid != entry.volume_valid)
                            || (entry.volume_valid && (!pa_cvolume_equal(&entry.volume, &old->volume)
                                                       || !pa_channel_map_equal(&entry.channel_map, &old->channel_map))))
                            send_volume_updated_signal(de, &entry);

                        if (!old->muted_valid || (entry.muted != old->muted))
                            send_mute_updated_signal(de, &entry);

                    } else {
                        de = dbus_entry_new(u, name);
                        pa_assert_se(pa_hashmap_put(u->dbus_entries, de->entry_name, de) == 0);
                        send_new_entry_signal(de);
                    }

                    if (u->use_sink_volume) {
                        _sink_set_volume(u->use_sink_volume->sink, &entry.volume);
                    } else {
                        if (apply_immediately)
                            apply_entry(u, name, &entry);
                    }
                }

                if (old)
                    pa_xfree(old);
            }

            /* no need to save volumes now if sink_volume-mode is on,
             * since they are saved later on anyway, in sink_volume_subscribe_cb
             */
            if (!u->use_sink_volume)
                trigger_save(u);

            break;
        }

        case SUBCOMMAND_DELETE:

            while (!pa_tagstruct_eof(t)) {
                const char *name;
                pa_datum key;
                struct dbus_entry *de;

                if (pa_tagstruct_gets(t, &name) < 0)
                    goto fail;

                if ((de = pa_hashmap_get(u->dbus_entries, name))) {
                    send_entry_removed_signal(de);
                    dbus_entry_free(pa_hashmap_remove(u->dbus_entries, name));
                }

                key.data = (char*) name;
                key.size = strlen(name);

                pa_database_unset(u->database, &key);
            }

            trigger_save(u);

            break;

        case SUBCOMMAND_SUBSCRIBE: {

            pa_bool_t enabled;

            if (pa_tagstruct_get_boolean(t, &enabled) < 0 ||
                !pa_tagstruct_eof(t))
                goto fail;

            if (enabled)
                pa_idxset_put(u->subscribed, c, NULL);
            else
                pa_idxset_remove_by_data(u->subscribed, c, NULL);

            break;
        }

        default:
            goto fail;
    }

    pa_pstream_send_tagstruct(pa_native_connection_get_pstream(c), reply);
    return 0;

fail:

    if (reply)
        pa_tagstruct_free(reply);

    return -1;
}

static pa_hook_result_t connection_unlink_hook_cb(pa_native_protocol *p, pa_native_connection *c, struct userdata *u) {
    pa_assert(p);
    pa_assert(c);
    pa_assert(u);

    pa_idxset_remove_by_data(u->subscribed, c, NULL);
    return PA_HOOK_OK;
}

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;
    char *fname;
    pa_sink_input *si;
    pa_source_output *so;
    uint32_t idx;
    pa_bool_t restore_device = TRUE,
              restore_volume = TRUE,
              restore_muted = TRUE,
              restore_route_volume = TRUE,
              on_hotplug = TRUE,
              on_rescue = TRUE;
    pa_datum key;
    pa_bool_t done;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    if (pa_modargs_get_value_boolean(ma, "restore_device", &restore_device) < 0 ||
        pa_modargs_get_value_boolean(ma, "restore_volume", &restore_volume) < 0 ||
        pa_modargs_get_value_boolean(ma, "restore_muted", &restore_muted) < 0 ||
        pa_modargs_get_value_boolean(ma, "restore_route_volume", &restore_route_volume) < 0 ||
        pa_modargs_get_value_boolean(ma, "on_hotplug", &on_hotplug) < 0 ||
        pa_modargs_get_value_boolean(ma, "on_rescue", &on_rescue) < 0) {
        pa_log("restore_device=, restore_volume=, restore_muted=, restore_route_volume=, on_hotplug= and on_rescue= expect boolean arguments");
        goto fail;
    }

    if (!restore_muted && !restore_volume && !restore_device)
        pa_log_warn("Neither restoring volume, nor restoring muted, nor restoring device enabled!");

    m->userdata = u = pa_xnew0(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->restore_device = restore_device;
    u->restore_volume = restore_volume;
    u->restore_muted = restore_muted;
    u->on_hotplug = on_hotplug;
    u->on_rescue = on_rescue;
    u->restore_route_volume = restore_route_volume;

    u->volume_proxy = pa_volume_proxy_get(u->core);
    u->volume_proxy_hook_slot = pa_hook_connect(&pa_volume_proxy_hooks(u->volume_proxy)[PA_VOLUME_PROXY_HOOK_CHANGED], PA_HOOK_NORMAL, (pa_hook_cb_t)volume_proxy_cb, u);

    u->subscribed = pa_idxset_new(pa_idxset_trivial_hash_func, pa_idxset_trivial_compare_func);

    u->protocol = pa_native_protocol_get(m->core);
    pa_native_protocol_install_ext(u->protocol, m, extension_cb);

    u->connection_unlink_hook_slot = pa_hook_connect(&pa_native_protocol_hooks(u->protocol)[PA_NATIVE_HOOK_CONNECTION_UNLINK], PA_HOOK_NORMAL, (pa_hook_cb_t) connection_unlink_hook_cb, u);

    u->subscription = pa_subscription_new(m->core, PA_SUBSCRIPTION_MASK_SINK_INPUT|PA_SUBSCRIPTION_MASK_SOURCE_OUTPUT, subscribe_callback, u);

    if (restore_device) {
        /* A little bit earlier than module-intended-roles ... */
        u->sink_input_new_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_NEW], PA_HOOK_EARLY, (pa_hook_cb_t) sink_input_new_hook_callback, u);
        u->source_output_new_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_OUTPUT_NEW], PA_HOOK_EARLY, (pa_hook_cb_t) source_output_new_hook_callback, u);
    }

    if (restore_device && on_hotplug) {
        /* A little bit earlier than module-intended-roles ... */
        u->sink_put_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PUT], PA_HOOK_LATE, (pa_hook_cb_t) sink_put_hook_callback, u);
        u->source_put_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_PUT], PA_HOOK_LATE, (pa_hook_cb_t) source_put_hook_callback, u);
    }

    if (restore_device && on_rescue) {
        /* A little bit earlier than module-intended-roles, module-rescue-streams, ... */
        u->sink_unlink_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_LATE, (pa_hook_cb_t) sink_unlink_hook_callback, u);
        u->source_unlink_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SOURCE_UNLINK], PA_HOOK_LATE, (pa_hook_cb_t) source_unlink_hook_callback, u);
    }

    if (restore_volume || restore_muted)
        u->sink_input_fixate_hook_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_FIXATE], PA_HOOK_EARLY, (pa_hook_cb_t) sink_input_fixate_hook_callback, u);

    if (u->restore_route_volume) {
        u->sink_proplist_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_PROPLIST_CHANGED], PA_HOOK_LATE, (pa_hook_cb_t)sink_proplist_changed_hook_callback, u);
        u->sink_input_move_finished_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_SINK_INPUT_MOVE_FINISH], PA_HOOK_NORMAL, (pa_hook_cb_t)hw_sink_input_move_finish_callback, u);
    }

    u->route = NULL;

    if (!(fname = pa_state_path("stream-volumes", TRUE)))
        goto fail;

    if (!(u->database = pa_database_open(fname, TRUE))) {
        pa_log("Failed to open volume database '%s': %s", fname, pa_cstrerror(errno));
        pa_xfree(fname);
        goto fail;
    }

    pa_log_info("Sucessfully opened database file '%s'.", fname);
    pa_xfree(fname);

    if (fill_db(u, pa_modargs_get_value(ma, "fallback_table", NULL)) < 0)
        goto fail;

    if (fill_route_db(u, pa_modargs_get_value(ma, "route_table", NULL)) < 0) {
        pa_log_debug("no route table found, route volumes disabled.\n");
    }

    if (fill_sink_db(u, pa_modargs_get_value(ma, "sink_volume_table", NULL))) {
        pa_log_debug("no sink volume table found, sink volumes disabled.\n");
    }

    u->dbus_protocol = pa_dbus_protocol_get(u->core);
    u->dbus_entries = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);

    pa_assert_se(pa_dbus_protocol_add_interface(u->dbus_protocol, OBJECT_PATH, &stream_restore_interface_info, u) >= 0);
    pa_assert_se(pa_dbus_protocol_register_extension(u->dbus_protocol, INTERFACE_STREAM_RESTORE) >= 0);

    /* Create the initial dbus entries. */
    done = !pa_database_first(u->database, &key, NULL);
    while (!done) {
        pa_datum next_key;
        char *name;
        struct dbus_entry *de;
        struct entry *e;

        done = !pa_database_next(u->database, &key, &next_key, NULL);

        name = pa_xstrndup(key.data, key.size);
        pa_datum_free(&key);

        /* Use read_entry() for checking that the entry is valid. */
        if ((e = read_entry(u, name))) {
            de = dbus_entry_new(u, name);
            pa_assert_se(pa_hashmap_put(u->dbus_entries, de->entry_name, de) == 0);
            pa_xfree(e);
        }

        pa_xfree(name);

        key = next_key;
    }

    if (!(fname = pa_state_path("x-maemo-route-volumes", TRUE)))
        goto fail;

    if (!(u->route_database = pa_database_open(fname, TRUE))) {
        pa_log("Failed to open volume database '%s': %s", fname, pa_cstrerror(errno));
        pa_xfree(fname);
        goto fail;
    }

    pa_log_info("Sucessfully opened database file '%s'.", fname);
    pa_xfree(fname);

    PA_IDXSET_FOREACH(si, m->core->sink_inputs, idx)
        subscribe_callback(m->core, PA_SUBSCRIPTION_EVENT_SINK_INPUT|PA_SUBSCRIPTION_EVENT_NEW, si->index, u);

    PA_IDXSET_FOREACH(so, m->core->source_outputs, idx)
        subscribe_callback(m->core, PA_SUBSCRIPTION_EVENT_SOURCE_OUTPUT|PA_SUBSCRIPTION_EVENT_NEW, so->index, u);

    pa_modargs_free(ma);
    return 0;

fail:
    pa__done(m);

    if (ma)
        pa_modargs_free(ma);

    return  -1;
}

static void free_dbus_entry_cb(void *p, void *userdata) {
    struct dbus_entry *de = p;

    pa_assert(de);

    dbus_entry_free(de);
}

void pa__done(pa_module*m) {
    struct userdata* u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->dbus_protocol) {
        pa_assert(u->dbus_entries);

        pa_assert_se(pa_dbus_protocol_unregister_extension(u->dbus_protocol, INTERFACE_STREAM_RESTORE) >= 0);
        pa_assert_se(pa_dbus_protocol_remove_interface(u->dbus_protocol, OBJECT_PATH, stream_restore_interface_info.name) >= 0);

        pa_hashmap_free(u->dbus_entries, free_dbus_entry_cb, NULL);

        pa_dbus_protocol_unref(u->dbus_protocol);
    }

    if (u->subscription)
        pa_subscription_free(u->subscription);

    if (u->sink_subscription)
        pa_subscription_free(u->sink_subscription);

    if (u->sink_input_new_hook_slot)
        pa_hook_slot_free(u->sink_input_new_hook_slot);
    if (u->sink_input_fixate_hook_slot)
        pa_hook_slot_free(u->sink_input_fixate_hook_slot);
    if (u->source_output_new_hook_slot)
        pa_hook_slot_free(u->source_output_new_hook_slot);
    if (u->sink_proplist_changed_slot)
        pa_hook_slot_free(u->sink_proplist_changed_slot);
    if (u->sink_input_move_finished_slot)
        pa_hook_slot_free(u->sink_input_move_finished_slot);

    if (u->sink_put_hook_slot)
        pa_hook_slot_free(u->sink_put_hook_slot);
    if (u->source_put_hook_slot)
        pa_hook_slot_free(u->source_put_hook_slot);

    if (u->sink_unlink_hook_slot)
        pa_hook_slot_free(u->sink_unlink_hook_slot);
    if (u->source_unlink_hook_slot)
        pa_hook_slot_free(u->source_unlink_hook_slot);

    if (u->connection_unlink_hook_slot)
        pa_hook_slot_free(u->connection_unlink_hook_slot);

    if (u->volume_proxy_hook_slot)
        pa_hook_slot_free(u->volume_proxy_hook_slot);
    if (u->volume_proxy)
        pa_volume_proxy_unref(u->volume_proxy);

    if (u->save_time_event)
        u->core->mainloop->time_free(u->save_time_event);

    if (u->database)
        pa_database_close(u->database);

    if (u->route_database)
        pa_database_close(u->route_database);

    if (u->protocol) {
        pa_native_protocol_remove_ext(u->protocol, m);
        pa_native_protocol_unref(u->protocol);
    }

    if (u->subscribed)
        pa_idxset_free(u->subscribed, NULL, NULL);

    _free_route_volumes(u);

    _free_sink_volumes(u);

    pa_xfree(u->route);
    pa_xfree(u);
}
