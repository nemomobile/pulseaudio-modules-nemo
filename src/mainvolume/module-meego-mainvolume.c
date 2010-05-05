/*
 * This file is part of pulseaudio-meego
 *
 * Copyright (C) 2010 Nokia Corporation. All rights reserved.
 *
 * Contact: Maemo Multimedia <multimedia@maemo.org>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved.
 *
 * Copying, including reproducing, storing, adapting or translating,
 * any or all of this material requires the prior written consent of
 * Nokia Corporation. This material also contains confidential
 * information which may not be disclosed to others without the prior
 * written consent of Nokia.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/core.h>
#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/core-util.h>
#include <pulsecore/protocol-dbus.h>
#include <pulsecore/dbus-util.h>
#include <pulsecore/hook-list.h>
#include <pulsecore/idxset.h>
#include <pulsecore/hashmap.h>

#include <pulsecore/call-state-tracker.h>
#include <pulsecore/volume-proxy.h>

#include "module-meego-mainvolume-symdef.h"
#include "mainvolume.h"

#include <src/common/parameter-hook.h>
#include <src/common/proplist-meego.h>

PA_MODULE_AUTHOR("Juho Hämäläinen");
PA_MODULE_DESCRIPTION("Nokia mainvolume module");
PA_MODULE_USAGE("tuning_mode=<true/false> defaults to false");
PA_MODULE_VERSION(PACKAGE_VERSION);

static const char* const valid_modargs[] = {
    "tuning_mode",
    NULL,
};

static void dbus_init(struct mv_userdata *u);
static void dbus_done(struct mv_userdata *u);
static void dbus_signal_steps(struct mv_userdata *u);

#define XMAEMO_CALL_STEPS "x-maemo.mainvolume.call"
#define XMAEMO_MEDIA_STEPS "x-maemo.mainvolume.media"

static void steps_set_free(struct mv_volume_steps_set *s, void *userdata) {
    pa_assert(s);

    pa_xfree(s->route);
    pa_xfree(s);
}

static pa_hook_result_t call_state_cb(pa_call_state_tracker *t, void *active, struct mv_userdata *u) {
    u->call_active = pa_call_state_tracker_get_active(t);

    return PA_HOOK_OK;
}

/* create new volume steps set for route with linear steps.
 * route        - name of step route
 * call_steps   - number of steps for call case
 * media_steps  - number of steps for media case
 */
static struct mv_volume_steps_set* fallback_new(const char *route, const int call_steps, const int media_steps) {
    struct mv_volume_steps_set *fallback;
    int i;

    pa_assert(route);

    fallback = pa_xnew0(struct mv_volume_steps_set, 1);
    fallback->call.n_steps = call_steps;
    fallback->media.n_steps = media_steps;

    /* calculate call/media_steps linearly using PA_VOLUME_NORM
     * as max value, starting from 0 volume. */

    for (i = 0; i < call_steps; i++)
        fallback->call.step[i] = (PA_VOLUME_NORM / call_steps-1)*i;

    for (i = 0; i < media_steps; i++)
        fallback->media.step[i] = (PA_VOLUME_NORM / media_steps-1)*i;

    fallback->route = pa_xstrdup(route);

    return fallback;
}

static pa_hook_result_t mode_changed_cb(pa_core *c, const char *mode, struct mv_userdata *u) {
    pa_assert(u);

    if (mode) {
        if (u->route) {
            if (pa_streq(u->route, mode))
                return PA_HOOK_OK;

            pa_xfree(u->route);
        }
        u->route = pa_xstrdup(mode);
        pa_log_debug("mode changes to %s", u->route);
    }

    return PA_HOOK_OK;
}

static pa_hook_result_t parameters_changed_cb(pa_core *c, struct update_args *ua, struct mv_userdata *u) {
    struct mv_volume_steps_set *set;
    pa_proplist *p = NULL;
    int ret = 0;

    pa_assert(u);

    if (!u->route || !ua || (ua && !ua->parameters))
        return PA_HOOK_OK;

    /* in tuning mode we always update steps when changing
     * x-maemo.mode.
     * First remove tunings in current route, then try to parse
     * normally */
    if (u->tuning_mode) {
        if ((set = pa_hashmap_remove(u->steps, u->route))) {
            steps_set_free(set, NULL);
            set = NULL;
        }
    }

    /* try to get step configuration from cache (hashmap) and
     * if steps aren't found try to parse them from property
     * list.
     * If no tunings can be found from property list or the tunings
     * are incorrect, we use "fallback" route, which is created
     * in module init.
     */
    set = pa_hashmap_get(u->steps, u->route);
    if (set) {
        u->current_steps = set;
    } else {
        if ((p = pa_proplist_from_string(ua->parameters)))
            ret = mv_parse_steps(u,
                                 u->route,
                                 pa_proplist_gets(p, XMAEMO_CALL_STEPS),
                                 pa_proplist_gets(p, XMAEMO_MEDIA_STEPS));

        if (ret > 0) {
            u->current_steps = pa_hashmap_get(u->steps, u->route);
        } else {
            pa_log_info("failed to update steps for %s, using fallback.", u->route);
            u->current_steps = pa_hashmap_get(u->steps, "fallback");
        }
    }

    if (p)
        pa_proplist_free(p);

    /* update steps for this route */
    mv_update_step(u);
    /* signal changed step settings forward */
    dbus_signal_steps(u);

    return PA_HOOK_OK;
}

static pa_hook_result_t volume_changed_cb(pa_volume_proxy *r, const char *name, struct mv_userdata *u) {
    pa_volume_t vol;
    struct mv_volume_steps *steps;
    int new_step;

    pa_assert(u);

    if (!pa_volume_proxy_get_volume(r, name, &vol))
        return PA_HOOK_OK;

    if (pa_streq(name, CALL_STREAM))
        steps = &u->current_steps->call;
    else
        steps = &u->current_steps->media;

    new_step = mv_search_step(steps->step, steps->n_steps, vol);

    if (new_step != steps->current_step) {
        pa_log_debug("volume changed for stream %s, vol %d new step %d", name, vol, new_step);

        steps->current_step = new_step;

        /* signal changed step forward */
        dbus_signal_steps(u);
    }

    return PA_HOOK_OK;
}

int pa__init(pa_module *m) {
    pa_modargs *ma = NULL;
    struct mv_userdata *u;
    struct mv_volume_steps_set *fallback;

    u = pa_xnew0(struct mv_userdata, 1);

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u;
    u->core = m->core;
    u->module = m;

    u->steps = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);

    fallback = fallback_new("fallback", 10, 20);
    pa_hashmap_put(u->steps, fallback->route, fallback);
    u->current_steps = fallback;

    u->tuning_mode = FALSE;

    if (pa_modargs_get_value_boolean(ma, "tuning_mode", &u->tuning_mode) < 0) {
        pa_log_error("tuning_mode expects boolean argument");
        goto fail;
    }

    u->call_state_tracker = pa_call_state_tracker_get(u->core);
    u->call_state_tracker_slot = pa_hook_connect(&pa_call_state_tracker_hooks(u->call_state_tracker)[PA_CALL_STATE_HOOK_CHANGED],
                                                 PA_HOOK_NORMAL,
                                                 (pa_hook_cb_t)call_state_cb,
                                                 u);

    u->volume_proxy = pa_volume_proxy_get(u->core);
    u->volume_proxy_slot = pa_hook_connect(&pa_volume_proxy_hooks(u->volume_proxy)[PA_VOLUME_PROXY_HOOK_CHANGED],
                                           PA_HOOK_NORMAL,
                                           (pa_hook_cb_t)volume_changed_cb,
                                           u);

    dbus_init(u);

    request_parameter_updates("mode", (pa_hook_cb_t)mode_changed_cb, PA_HOOK_NORMAL, u);
    request_parameter_updates("mainvolume", (pa_hook_cb_t)parameters_changed_cb, PA_HOOK_NORMAL, u);

    pa_modargs_free(ma);

    return 0;

 fail:
    if (ma)
        pa_modargs_free(ma);

    pa_xfree(u);
    m->userdata = NULL;

    return -1;
}

void pa__done(pa_module *m) {
    struct mv_userdata *u = m->userdata;

    dbus_done(u);

    if (u->sink_proplist_changed_slot)
        pa_hook_slot_free(u->sink_proplist_changed_slot);

    if (u->call_state_tracker_slot)
        pa_hook_slot_free(u->call_state_tracker_slot);

    if (u->call_state_tracker)
        pa_call_state_tracker_unref(u->call_state_tracker);

    if (u->volume_proxy_slot)
        pa_hook_slot_free(u->volume_proxy_slot);

    if (u->volume_proxy)
        pa_volume_proxy_unref(u->volume_proxy);

    pa_hashmap_free(u->steps, (pa_free2_cb_t)steps_set_free, NULL);

    pa_assert(m);

    if (u)
        pa_xfree(u);
}

/*
 * DBus
 */

#define MAINVOLUME_API_MAJOR (1)
#define MAINVOLUME_API_MINOR (0)
#define MAINVOLUME_PATH "/com/meego/mainvolume1"
#define MAINVOLUME_IFACE "com.Nokia.MainVolume1"

static void mainvolume_get_revision(DBusConnection *conn, DBusMessage *msg, void *_u);
static void mainvolume_get_step_count(DBusConnection *conn, DBusMessage *msg, void *_u);
static void mainvolume_get_current_step(DBusConnection *conn, DBusMessage *msg, void *_u);
static void mainvolume_set_current_step(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *_u);
static void mainvolume_get_all(DBusConnection *conn, DBusMessage *msg, void *_u);

enum mainvolume_handler_index {
    MAINVOLUME_HANDLER_REVISION,
    MAINVOLUME_HANDLER_STEP_COUNT,
    MAINVOLUME_HANDLER_CURRENT_STEP,
    MAINVOLUME_HANDLER_MAX
};

static pa_dbus_property_handler mainvolume_handlers[MAINVOLUME_HANDLER_MAX] = {
    [MAINVOLUME_HANDLER_REVISION] = {
        .property_name = "InterfaceRevision",
        .type = "u",
        .get_cb = mainvolume_get_revision,
        .set_cb = NULL
    },
    [MAINVOLUME_HANDLER_STEP_COUNT] = {
        .property_name = "StepCount",
        .type = "u",
        .get_cb = mainvolume_get_step_count,
        .set_cb = NULL
    },
    [MAINVOLUME_HANDLER_CURRENT_STEP] = {
        .property_name = "CurrentStep",
        .type = "u",
        .get_cb = mainvolume_get_current_step,
        .set_cb = mainvolume_set_current_step
    }
};

enum mainvolume_signal_index {
    MAINVOLUME_SIGNAL_STEPS_UPDATED,
    MAINVOLUME_SIGNAL_MAX
};

static pa_dbus_arg_info steps_updated_args[] = {
    {"StepCount", "u", "out"},
    {"CurrentStep", "u", "out"}
};

static pa_dbus_signal_info mainvolume_signals[MAINVOLUME_SIGNAL_MAX] = {
    [MAINVOLUME_SIGNAL_STEPS_UPDATED] = {
        .name = "StepsUpdated",
        .arguments = steps_updated_args,
        .n_arguments = 2
    }
};

static pa_dbus_interface_info mainvolume_info = {
    .name = MAINVOLUME_IFACE,
    .method_handlers = NULL,
    .n_method_handlers = 0,
    .property_handlers = mainvolume_handlers,
    .n_property_handlers = MAINVOLUME_HANDLER_MAX,
    .get_all_properties_cb = mainvolume_get_all,
    .signals = mainvolume_signals,
    .n_signals = MAINVOLUME_SIGNAL_MAX
};

void dbus_init(struct mv_userdata *u) {
    pa_assert(u);
    pa_assert(u->core);

    u->dbus_protocol = pa_dbus_protocol_get(u->core);
    u->dbus_path = pa_sprintf_malloc("/com/meego/mainvolume%d", MAINVOLUME_API_MAJOR);

    pa_dbus_protocol_add_interface(u->dbus_protocol, MAINVOLUME_PATH, &mainvolume_info, u);
    pa_dbus_protocol_register_extension(u->dbus_protocol, MAINVOLUME_IFACE);
}

void dbus_done(struct mv_userdata *u) {
    pa_assert(u);

    pa_dbus_protocol_unregister_extension(u->dbus_protocol, MAINVOLUME_IFACE);
    pa_dbus_protocol_remove_interface(u->dbus_protocol, u->dbus_path, mainvolume_info.name);
    pa_xfree(u->dbus_path);
    pa_dbus_protocol_unref(u->dbus_protocol);
}

void dbus_signal_steps(struct mv_userdata *u) {
    DBusMessage *signal;
    struct mv_volume_steps *steps;
    uint32_t current_step;
    uint32_t step_count;

    pa_assert(u);
    pa_assert(u->current_steps);

    steps = mv_active_steps(u);
    step_count = steps->n_steps;
    current_step = steps->current_step;

    pa_assert_se((signal = dbus_message_new_signal(MAINVOLUME_PATH,
                                                   MAINVOLUME_IFACE,
                                                   mainvolume_signals[MAINVOLUME_SIGNAL_STEPS_UPDATED].name)));
    pa_assert_se(dbus_message_append_args(signal,
                                          DBUS_TYPE_UINT32, &step_count,
                                          DBUS_TYPE_UINT32, &current_step,
                                          DBUS_TYPE_INVALID));
    pa_dbus_protocol_send_signal(u->dbus_protocol, signal);
    dbus_message_unref(signal);
}

void mainvolume_get_revision(DBusConnection *conn, DBusMessage *msg, void *_u) {
    uint32_t rev = MAINVOLUME_API_MINOR;
    pa_dbus_send_basic_value_reply(conn, msg, DBUS_TYPE_UINT32, &rev);
}

void mainvolume_get_step_count(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct mv_userdata *u = (struct mv_userdata*)_u;
    struct mv_volume_steps *steps;
    uint32_t step_count;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    steps = mv_active_steps(u);
    step_count = steps->n_steps;
    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &step_count);
}

void mainvolume_get_current_step(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct mv_userdata *u = (struct mv_userdata*)_u;
    struct mv_volume_steps *steps;
    uint32_t step;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    steps = mv_active_steps(u);
    step = steps->current_step;
    pa_dbus_send_basic_variant_reply(conn, msg, DBUS_TYPE_UINT32, &step);
}

void mainvolume_set_current_step(DBusConnection *conn, DBusMessage *msg, DBusMessageIter *iter, void *_u) {
    struct mv_userdata *u = (struct mv_userdata*)_u;
    struct mv_volume_steps *steps;
    uint32_t set_step;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    steps = mv_active_steps(u);

    dbus_message_iter_get_basic(iter,  &set_step);

    if (set_step >= steps->n_steps) {
        pa_dbus_send_error(conn, msg, DBUS_ERROR_INVALID_ARGS, "Step %d out of bounds.", set_step);
        return;
    }

    mv_set_step(u, set_step);

    pa_dbus_send_empty_reply(conn, msg);
}

void mainvolume_get_all(DBusConnection *conn, DBusMessage *msg, void *_u) {
    struct mv_userdata *u = (struct mv_userdata*)_u;
    DBusMessage *reply = NULL;
    DBusMessageIter msg_iter;
    DBusMessageIter dict_iter;
    uint32_t rev;
    struct mv_volume_steps *steps;
    uint32_t step_count;
    uint32_t current_step;

    pa_assert(conn);
    pa_assert(msg);
    pa_assert(u);

    steps = mv_active_steps(u);

    rev = MAINVOLUME_API_MINOR;
    step_count = steps->n_steps;
    current_step = steps->current_step;

    pa_assert_se((reply = dbus_message_new_method_return(msg)));
    dbus_message_iter_init_append(reply, &msg_iter);
    pa_assert_se(dbus_message_iter_open_container(&msg_iter, DBUS_TYPE_ARRAY, "{sv}", &dict_iter));

    pa_dbus_append_basic_variant_dict_entry(&dict_iter,
                                            mainvolume_handlers[MAINVOLUME_HANDLER_REVISION].property_name,
                                            DBUS_TYPE_UINT32, &rev);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter,
                                            mainvolume_handlers[MAINVOLUME_HANDLER_STEP_COUNT].property_name,
                                            DBUS_TYPE_UINT32, &step_count);
    pa_dbus_append_basic_variant_dict_entry(&dict_iter,
                                            mainvolume_handlers[MAINVOLUME_HANDLER_CURRENT_STEP].property_name,
                                            DBUS_TYPE_UINT32, &current_step);

    pa_assert_se(dbus_message_iter_close_container(&msg_iter, &dict_iter));
    pa_assert_se(dbus_connection_send(conn, reply, NULL));
    dbus_message_unref(reply);
}

