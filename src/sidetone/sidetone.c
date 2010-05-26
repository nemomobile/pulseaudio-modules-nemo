/*
 * Copyright (C) 2010 Nokia Corporation.
 *
 * Contact: Maemo MMF Audio <mmf-audio@projects.maemo.org>
 *          or Jyri Sarha <jyri.sarha@nokia.com>
 *
 * These PulseAudio Modules are free software; you can redistribute
 * it and/or modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA.
 */

/* A brief explanation of what this code does:

  When sidetone is enabled (a call is active and sidetone is enabled in the
  current audio mode), this code attempts to keep the volume of the sidetone path
  (i.e. a collection of ALSA mixer elements) close to constant by adjusting the
  volume of a specific ALSA element in the sidetone path (i.e.  the control
  element). Given a desired sum of dB values ('target_volume'), this code
  maintains the sum of the mixer element volumes excluding the control element
  ('total_volume') and requests the control element to set its volume to
  'target_volume' - 'total_volume'. The control element attempts to set its
  volume close to this value. The resulting volume depends on the available
  volume range and steps of the control element.

  During creation (sidetone_new):

    * Get the volumes for each sidetone element (input and output). Sum them
      up as 'total_volume'.
    * Store each element volume by name in the 'element_volumes' hashmap
    * Create the control element, i.e. the element used to control the sidetone volume.
    * Scan configured sink(s) (and optionally source(s)). This means:
        - Request the ALSA paths from the sinks/sources
        - Scan each path. I.e.:
            + Go through each path element and see if it's in 'element_volumes'
               # If it is, request its volume from ALSA and update it to
                 'element_volumes'. Also recalculate 'total_volume'.
               # Yes, it's a bit redundant to request the volumes again at
                 this point, but the 'scan_path' function is reused in many
                 parts where it actually is necessary to request the volumes again.
            + If the path contained elements that were in 'element_volumes',
              store a pointer to the path for future use. Also start listening
              to the path's volume hook.
    * Attach to the unlink hooks of the sinks/sources. This is just to be safe
      in case the sinks/sources are unlinked, in which case their paths become
      dangling pointers.
    * Start listening to call state changes using 'call_state_tracker'.
    * Mute the sidetone control element.

  Normal operation:

    * Sidetone is enabled when non-null parameters have been received through
      the parameter callback and a call is active.
       - If sidetone goes from disabled to enabled, each stored path is
         re-scanned using 'scan_path' and the control element volume is set.
    * If the volume of a stored path changes, 'path_volume_changed_cb' is
      called. If sidetone is enabled, the changed path is scanned using
      'scan_path' and the control element volume is set. Otherwise nothing is done.
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/call-state-tracker.h>
#include <pulsecore/namereg.h>
#include <pulsecore/core-util.h>
#include <pulsecore/mutex.h>

#include <pulsecore/modules/alsa/alsa-mixer.h>
#include <pulsecore/modules/alsa/alsa-util-old.h>
#include <pulsecore/modules/alsa/alsa-sink-old.h>
#include <pulsecore/modules/alsa/alsa-sink.h>
#include <pulsecore/modules/alsa/alsa-source-old.h>
#include <pulsecore/modules/alsa/alsa-source.h>

#include <src/common/parameter-hook.h>
#include <src/common/proplist-meego.h>

#include "ctrl-element.h"
#include "alsa-utils.h"
#include "sidetone-args.h"
#include "sidetone.h"

/* Describes the volume of an element in the sideone path. */
struct element_volume {
    long volume;
    snd_mixer_selem_channel_id_t channel; /* Sidetone is mono, so we're only interested in one channel (default SND_MIXER_SCHN_MONO) */
    pa_bool_t playback; /* Is this a playback or capture element? */
};

/* Sidetone object data */
struct sidetone {
    pa_call_state_tracker *call_state_tracker; /* Call state tracker is used to determine if a call is active */
    pa_hook_slot *call_state_tracker_slot;

    snd_mixer_t *mixer; /* The mixer that contains the elements of the sidetone loop */

    ctrl_element *ctrl_element; /* The element used for controlling the sidetone loop volume */

    pa_bool_t enabled_in_mode; /* Is sidetone enabled in the current audio mode? */
    pa_bool_t call_active; /* Is a call currently active? */

    /* If important sinks or sources are unlinked unexpectedly, paths owned by
     * them are destroyed. This flag is set in such cases to disable the
     * sidetone altogether. In addition, enabled_in_mode and call_active are
     * set as "FALSE" to prevent any access to possible dangling path pointers.
     * When sidetone parameters or the call state change, this flag prevents
     * enabled_in_mode and call_active from being set. */
    pa_bool_t dead;

    long target_volume; /* The sidetone loop volume we want to achieve by adjusting the control element */
    long total_volume;  /* The total volume of the sidetone loop excluding the control element */

    pa_hashmap *element_volumes; /* Individual element volumes (indexed by element name) used for updating the total volume */

    pa_sink **sinks ; /* The sinks whose element paths we want to monitor */
    int num_sinks;
    pa_hook_slot* sink_unlink_slot; /* Sink unlink slot is needed in case important sinks disappear */

    pa_source **sources; /* The sources whose element paths we want to monitor */
    int num_sources;
    pa_hook_slot* source_unlink_slot;

    pa_alsa_path **paths; /* The paths that we monitor for volume changes */
    int num_paths; /* Current number of path pointers */
    int max_paths; /* Space currently allocated for path pointers */

    PA_LLIST_HEAD(pa_hook_slot, volume_slots); /* Volume slots for the paths' volume hooks */

    /* The callbacks implemented by the sidetone object are called from both
     * the main thread as well as the IO threads of ALSA sinks and sources.
     * Thus, we need to lock this mutex whenever we are touching the internals
     * of the sidetone object inside callbacks. */
    pa_mutex *mutex;

    /* Input and output element names are stored because we're using them as
     * hashmap keys and don't want to copy them around */
    const char **input_element_names;
    int num_input_elements;
    const char **output_element_names;
    int num_output_elements;
};

/* Go through each element in a path and update the total volume */
static pa_bool_t scan_path(sidetone *st, pa_alsa_path *path) {
    pa_assert(st);
    pa_assert(path);
    pa_assert(!st->dead);

    pa_alsa_element *pa_element = NULL;
    struct element_volume *element_volume = NULL;
    long new_volume = 0;
    pa_bool_t is_relevant_path = FALSE;

    pa_log_debug("Scanning path \"%s\"", path->name);

    PA_LLIST_FOREACH(pa_element, path->elements) {
        pa_log_debug("Scanning element \"%s\"", pa_element->alsa_name);

        if(!(element_volume = pa_hashmap_get(st->element_volumes, pa_element->alsa_name))) {
            pa_log_debug("Element \"%s\" not configured as a sidetone element", pa_element->alsa_name);
            continue;
        }

        is_relevant_path = TRUE;

        pa_assert_se(mixer_get_element_volume(st->mixer, pa_element->alsa_name, element_volume->channel,
                                              element_volume->playback, &new_volume) >= 0);

        pa_log_debug("Element \"%s\" volume = %ld", pa_element->alsa_name, new_volume);

        if(new_volume != element_volume->volume) {
            st->total_volume -= element_volume->volume;
            st->total_volume += new_volume;
            element_volume->volume = new_volume;
        }
    }

    pa_log_debug("Path scanned. Total volume without sidetone control element = %ld, "
                 "Target volume = %ld", st->total_volume, st->target_volume);

    return is_relevant_path;
}

/* Called from an IO thread
 * Called when the volume hook for a path is fired */
static pa_hook_result_t path_volume_changed_cb(pa_alsa_path *path, void *call_data, sidetone *st) {
    pa_assert(path);
    pa_assert(st);

    pa_mutex_lock(st->mutex);

    if(st->enabled_in_mode && st->call_active) {
        scan_path(st, path);
        ctrl_element_set_volume(st->ctrl_element, st->target_volume - st->total_volume);
    }

    pa_mutex_unlock(st->mutex);

    return PA_HOOK_OK;
}

/* Store a path, connect a volume hook for the path and store the associated slot */
static void store_path(sidetone *st, pa_alsa_path *path) {
    pa_assert(st);
    pa_assert(path);
    pa_assert(!st->dead);

    pa_hook_slot *slot = NULL;

    if(!st->paths) {
        st->max_paths = 4; /* Arbitrary constant for the initial array size */
        st->paths = pa_xmalloc0(st->max_paths * sizeof(pa_alsa_path*));
    }

    if(st->num_paths >= st->max_paths) {
        st->max_paths *= 2;
        st->paths = pa_xrealloc(st->paths, st->max_paths * sizeof(pa_alsa_path*));
    }

    st->paths[st->num_paths] = path;
    st->num_paths++;

    slot = pa_hook_connect(&path->hooks[PA_ALSA_PATH_HOOK_VOLUME_CHANGED], PA_HOOK_NORMAL,
                           (pa_hook_cb_t)path_volume_changed_cb, st);

    PA_LLIST_PREPEND(pa_hook_slot, st->volume_slots, slot);

    pa_log_debug("Connected volume hook for path \"%s\"", path->name);
}

/* Scan all paths of an alsa sink. Store interesting paths to our list and start listening to their volume hooks. */
static int scan_sink(sidetone *st, pa_sink *sink) {
    pa_assert(st);
    pa_assert(sink);
    pa_assert(!st->dead);

    const char *api_property = NULL;
    pa_alsa_path_set *path_set = NULL;
    pa_alsa_path *path = NULL;
    pa_bool_t is_relevant_sink = FALSE;

    pa_log_debug("Scanning sink %s", sink->name);

    api_property = pa_proplist_gets(sink->proplist, PA_PROP_DEVICE_API);
    if(!api_property || !pa_streq(api_property, "alsa")) {
        pa_log_error("Sink %s is not a valid alsa sink (property %s missing or incorrect)", sink->name, PA_PROP_DEVICE_API);
        return -1;
    }

    /* The api variant property tells us which function to use for querying paths */
    api_property = pa_proplist_gets(sink->proplist, PA_PROP_ALSA_VARIANT);

    if(api_property && pa_streq(api_property, "old")) {
        pa_alsa_sink_old_get_paths(sink, &path_set, &path);
    } else {
        pa_alsa_sink_get_paths(sink, &path_set, &path);
    }

    /* We now have either 'path_set' or 'path'. Go through each (or the single)
     * path. If there are paths that contain elements that affect the sidetone
     * volume, store them to our 'paths' array and start listening to their
     * volume hooks. */ 
    if(path_set) {
        PA_LLIST_FOREACH(path, path_set->paths) {
            if(scan_path(st, path)) {
                store_path(st, path);
                is_relevant_sink = TRUE;
            }
        }
    } else if(scan_path(st, path)) {
        store_path(st, path);
        is_relevant_sink = TRUE;
    }

    if(!is_relevant_sink) {
        pa_log_error("Sink %s did not contain any sidetone elements!", sink->name);
        return -1;
    }

    pa_log_debug("Sink %s scanned", sink->name);

    return 0;
}

/* Identical to scan_sink, but for sources */
static int scan_source(sidetone *st, pa_source *source) {
    pa_assert(st);
    pa_assert(source);
    pa_assert(!st->dead);

    const char *api_property = NULL;
    pa_alsa_path_set *path_set = NULL;
    pa_alsa_path *path = NULL;
    pa_bool_t is_relevant_source = FALSE;

    pa_log_debug("Scanning source %s", source->name);

    api_property = pa_proplist_gets(source->proplist, PA_PROP_DEVICE_API);
    if(!api_property || !pa_streq(api_property, "alsa")) {
        pa_log_error("Source %s is not a valid alsa source (property %s missing or incorrect)", source->name, PA_PROP_DEVICE_API);
        return -1;
    }

    api_property = pa_proplist_gets(source->proplist, PA_PROP_ALSA_VARIANT);

    if(api_property && pa_streq(api_property, "old")) {
        pa_alsa_source_old_get_paths(source, &path_set, &path);
    } else {
        pa_alsa_source_get_paths(source, &path_set, &path);
    }

   if(path_set) {
       PA_LLIST_FOREACH(path, path_set->paths) {
            if(scan_path(st, path)) {
                store_path(st, path);
                is_relevant_source = TRUE;
            }
        }
    } else if(scan_path(st, path)) {
        store_path(st, path);
        is_relevant_source = TRUE;
    }

    if(!is_relevant_source) {
        pa_log_error("Source %s did not contain any sidetone elements!", source->name);
        return -1;
    }

    pa_log_debug("Source %s scanned", source->name);

    return 0;
}

/* Update the sidetone state based on previous and currently enabled flags. */
static void update_sidetone_state(sidetone *st, pa_bool_t was_enabled) {
    pa_assert(st);
    pa_assert(!st->dead);

    if(st->enabled_in_mode && st->call_active) {
        if(!was_enabled) {
            /* Sidetone wasn't enabled before this, so our volumes are probably
             * not up to date. Thus, we need to scan all the relevant paths. */
            int i;
            for(i = 0; i < st->num_paths; i++) {
                scan_path(st, st->paths[i]);
            }
        }
        ctrl_element_set_volume(st->ctrl_element, st->target_volume - st->total_volume);
    } else {
        ctrl_element_mute(st->ctrl_element);
    }
}

/* Called from the main thread
 * Sidetone parameter callback. */
static pa_hook_result_t parameters_changed_cb(pa_core *c, struct update_args *ua, sidetone *st) {
    pa_assert(st);

    pa_mutex_lock(st->mutex);

    if(st->dead) {
        pa_log_warn("Parameter hook called, but the sidetone module is dead!");
    } else {
        pa_bool_t was_enabled = st->enabled_in_mode && st->call_active;
        st->enabled_in_mode = (ua != NULL);
        update_sidetone_state(st, was_enabled);
    }

    pa_mutex_unlock(st->mutex);

    return PA_HOOK_OK;
}

/* Called from the main thread
 * Called by call state tracker. */
static pa_hook_result_t call_state_changed_cb(pa_call_state_tracker *tracker, pa_bool_t active, sidetone *st) {
    pa_assert(tracker);
    pa_assert(st);

    pa_mutex_lock(st->mutex);

    if(st->dead) {
        pa_log_warn("Call state changed, but the sidetone module is dead!");
    } else {
        pa_bool_t was_enabled = st->enabled_in_mode && st->call_active;
        st->call_active = active;
        update_sidetone_state(st, was_enabled);
    }

    pa_mutex_unlock(st->mutex);

    return PA_HOOK_OK;
}

/* Sets sidetone in a disabled state and mutes the control element. Only used
 * in exceptional circumstances where an important sink or source is unlinked
 * unexpectedly. */
static void set_dead(sidetone *st) {
    pa_assert(st);

    st->dead = TRUE;
    st->enabled_in_mode = FALSE;
    st->call_active = FALSE;
    ctrl_element_mute(st->ctrl_element);
}

/* Called from the main thread
 * If an alsa sink whose paths we're monitoring is unlinked, we need to disable
 * sidetone, because the paths associated with the sink are no longer valid. */
static pa_hook_result_t sink_unlink_cb(pa_sink *sink, void *call_data, sidetone *st) {
    pa_assert(sink);
    pa_assert(st);
    int i;

    pa_mutex_lock(st->mutex);

    for(i = 0; i < st->num_sinks; i++) {
        if(sink == st->sinks[i]) {
            pa_log_warn("An important sink (%s) was unlinked. Disabling sidetone module.", sink->name);
            set_dead(st);
            break;
        }
    }

    pa_mutex_unlock(st->mutex);

    return PA_HOOK_OK;
}

/* Called from the main thread
 * Similar to sink_unlink_cb */
static pa_hook_result_t source_unlink_cb(pa_source *source, void *call_data, sidetone *st) {
    pa_assert(source);
    pa_assert(st);
    int i;

    pa_mutex_lock(st->mutex);

    for(i = 0; i < st->num_sources; i++) {
        if(source == st->sources[i]) {
            pa_log_warn("An important source (%s) was unlinked. Disabling sidetone module.", source->name);
            set_dead(st);
            break;
        }
    }

    pa_mutex_unlock(st->mutex);

    return PA_HOOK_OK;
}

/* Get element volumes from the specified channels and store the information in
 * the element_volumes hashmap. element_names and channels must be of equal
 * length (num_elements) */
static int initialize_elements(sidetone *st, const char **element_names, const snd_mixer_selem_channel_id_t *channels,
                               int num_elements, pa_bool_t playback) {
    pa_assert(st);
    pa_assert(element_names);
    pa_assert(channels);

    int i = 0;
    struct element_volume *element_volume = NULL;
    long current_volume = 0;

    for(i = 0; i < num_elements; i++) {
        if(mixer_get_element_volume(st->mixer, element_names[i], channels[i], playback, &current_volume) < 0) {
            pa_log_error("Failed to initialize element \"%s\"", element_names[i]);
            return -1;
        }

        element_volume = pa_xnew0(struct element_volume, 1);
        element_volume->channel = channels[i];
        element_volume->playback = playback;
        element_volume->volume = current_volume;

        pa_hashmap_put(st->element_volumes, element_names[i], element_volume);

        st->total_volume += current_volume;

        pa_log_debug("Element \"%s\" initialized. Volume = %ld", element_names[i], current_volume);
    }

    pa_log_debug("Total volume = %ld", st->total_volume);

    return 0;
}

/* Create a new sidetone instance. argument contains the raw module args of the sidetone module */
sidetone *sidetone_new(pa_core *core, const char* argument) {
    pa_assert(core);
    pa_assert(argument);

    sidetone *st = NULL;
    sidetone_args *st_args = NULL;
    int i = 0;

    if(!(st_args = sidetone_args_new(argument))) {
        goto fail;
    }

    st = pa_xnew0(struct sidetone, 1);

    st->mutex = pa_mutex_new(FALSE, FALSE);

    if(!(st->mixer = pa_alsa_old_open_mixer(st_args->mixer))) {
        pa_log_error("Failed to open mixer \"%s\"", st_args->mixer);
        goto fail;
    }

    st->target_volume = st_args->target_volume;

    /* This needs to be created before element initialization */
    st->element_volumes = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);

    if(initialize_elements(st,
                           st_args->input_elements,
                           st_args->input_channels,
                           st_args->num_input_elements,
                           FALSE) < 0) {
        pa_log_error("Failed to initialize input elements");
        goto fail;
    }

    if(initialize_elements(st,
                           st_args->output_elements,
                           st_args->output_channels,
                           st_args->num_output_elements,
                           TRUE) < 0) {
        pa_log_error("Failed to initialize output elements");
        goto fail;
    }

    /* Slightly hackish. Change ownership of the element names because we're
     * using them as hashmap keys and don't want them freed in
     * sidetone_args_free. We also won't bother copying them around. */
    st->input_element_names = st_args->input_elements;
    st->num_input_elements = st_args->num_input_elements;
    st_args->input_elements = NULL;

    st->output_element_names = st_args->output_elements;
    st->num_output_elements = st_args->num_output_elements;
    st_args->output_elements = NULL;

    if(!(st->ctrl_element = ctrl_element_new(st->mixer, st_args->control_element))) {
        pa_log_error("Failed to create control element");
        goto fail;
    }

    /* At least one sink must be specified */
    pa_assert(st_args->num_sinks > 0);
    st->sinks = pa_xmalloc0(st_args->num_sinks * sizeof(pa_sink*));
    st->num_sinks = st_args->num_sinks;

    for(i = 0; i < st_args->num_sinks; i++) {
        pa_sink *sink = NULL;

        if(!(sink = pa_namereg_get(core, st_args->sinks[i], PA_NAMEREG_SINK))) {
            pa_log_error("Sink %s not found", st_args->sinks[i]);
            goto fail;
        }

        if(scan_sink(st, sink) < 0) {
            pa_log_error("Failed to scan sink %s", st_args->sinks[i]);
            goto fail;
        }

        st->sinks[i] = sink;
    }

    /* Sources are not mandatory */
    if(st_args->num_sources > 0) {
        st->sources = pa_xmalloc0(st_args->num_sources * sizeof(pa_source*));
        st->num_sources = st_args->num_sources;
    }

    for(i = 0; i < st_args->num_sources; i++) {
        pa_source *source = NULL;

        if(!(source = pa_namereg_get(core, st_args->sources[i], PA_NAMEREG_SOURCE))) {
            pa_log_error("Source %s not found", st_args->sources[i]);
            goto fail;
        }

        if(scan_source(st, source) < 0) {
            pa_log_error("Failed to scan source %s", st_args->sources[i]);
            goto fail;
        }

        st->sources[i] = source;
    }

    /* Attach to sink/source unlink hooks so that we can react if important sinks/sources are unlinked */
    st->sink_unlink_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SINK_UNLINK], PA_HOOK_NORMAL,
                                           (pa_hook_cb_t)sink_unlink_cb, st);
    st->source_unlink_slot = pa_hook_connect(&core->hooks[PA_CORE_HOOK_SOURCE_UNLINK], PA_HOOK_NORMAL,
                                             (pa_hook_cb_t)source_unlink_cb, st);

    pa_assert_se(st->call_state_tracker = pa_call_state_tracker_get(core));

    st->call_active = pa_call_state_tracker_get_active(st->call_state_tracker);

    st->call_state_tracker_slot = 
        pa_hook_connect(&pa_call_state_tracker_hooks(st->call_state_tracker)[PA_CALL_STATE_HOOK_CHANGED],
                        PA_HOOK_NORMAL,
                        (pa_hook_cb_t)call_state_changed_cb,
                        st);

    st->enabled_in_mode = FALSE;
    st->dead = FALSE;

    ctrl_element_mute(st->ctrl_element);

    pa_assert_se(request_parameter_updates("sidetone", (pa_hook_cb_t)parameters_changed_cb, PA_HOOK_NORMAL, st) >= 0);

    sidetone_args_free(st_args);

    return st;

fail:

    if(st_args)
        sidetone_args_free(st_args);

    if(st)
        sidetone_free(st);

    return NULL;
}

static void element_volume_free(struct element_volume *volume, void *userdata) {
    pa_assert(volume);
    pa_xfree(volume);
}

void sidetone_free(sidetone *st) {
    pa_assert(st);

    int i = 0;
    pa_hook_slot *slot = NULL;

    if(st->call_state_tracker_slot)
        pa_hook_slot_free(st->call_state_tracker_slot);

    if(st->call_state_tracker)
        pa_call_state_tracker_unref(st->call_state_tracker);

    if(st->ctrl_element)
        ctrl_element_free(st->ctrl_element);

    if(st->element_volumes)
        pa_hashmap_free(st->element_volumes, (pa_free2_cb_t)element_volume_free, NULL);

    if(st->sink_unlink_slot)
        pa_hook_slot_free(st->sink_unlink_slot);

    if(st->sinks)
        pa_xfree(st->sinks);

    if(st->source_unlink_slot)
        pa_hook_slot_free(st->source_unlink_slot);

    if(st->sources)
        pa_xfree(st->sources);

    if(st->paths)
        pa_xfree(st->paths);

    while((slot = st->volume_slots)) {
        st->volume_slots = st->volume_slots->next;
        pa_hook_slot_free(slot);
    }

    if(st->input_element_names) {
        for(i = 0; i < st->num_input_elements; i++) {
            pa_xfree(st->input_element_names[i]);
        }
        pa_xfree(st->input_element_names);
    }

    if(st->output_element_names) {
        for(i = 0; i < st->num_output_elements; i++) {
            pa_xfree(st->output_element_names[i]);
        }
        pa_xfree(st->output_element_names);
    }

    if(st->mutex)
        pa_mutex_free(st->mutex);

    pa_xfree(st);
}

