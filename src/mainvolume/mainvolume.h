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

#ifndef foomainvolumehfoo
#define foomainvolumehfoo

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/core.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/protocol-dbus.h>
#include <pulsecore/hook-list.h>
#include <pulsecore/strlist.h>

#include "call-state-tracker.h"
#include "volume-proxy.h"
#include "listening-watchdog.h"

#define MEDIA_STREAM "sink-input-by-media-role:x-maemo"
#define CALL_STREAM "sink-input-by-media-role:phone"
#define MAX_STEPS (64)

struct mv_volume_steps {
    int step[MAX_STEPS];
    unsigned n_steps;
    unsigned current_step;
};

struct mv_volume_steps_set {
    char *route;

    struct mv_volume_steps call;
    struct mv_volume_steps media;
    int high_volume_step;
    /* when parsing volume steps first is set TRUE,
     * and if entering a route with volume higher than high_volume_step,
     * volume is reset to safe volume.
     * This is done once per parsed steps, first is set
     * to FALSE after first check. */
    pa_bool_t first;
};

struct mv_userdata {
    pa_core *core;
    pa_module *module;
    pa_bool_t tuning_mode;
    pa_bool_t virtual_stream;

    pa_sink_input *virtual_sink_input;

    pa_hashmap *steps;
    struct mv_volume_steps_set *current_steps;
    char *route;

    pa_call_state_tracker *call_state_tracker;
    pa_hook_slot *call_state_tracker_slot;
    pa_bool_t call_active;

    pa_volume_proxy *volume_proxy;
    pa_hook_slot *volume_proxy_slot;

    pa_hook_slot *sink_proplist_changed_slot;

    pa_bool_t mode_change_ready;
    pa_bool_t volume_change_ready;

    pa_time_event *signal_time_event;
    pa_usec_t last_signal_timestamp;
    pa_usec_t last_step_set_timestamp;

    pa_dbus_protocol *dbus_protocol;
    char *dbus_path;

    struct notifier_data {
        mv_listening_watchdog *watchdog;
        pa_hook_slot *sink_input_put_slot;
        pa_hook_slot *sink_input_changed_slot;
        pa_hook_slot *sink_input_unlink_slot;
        uint32_t timeout;
        /* Modes that are watched. */
        pa_hashmap *modes;
        pa_bool_t mode_active;

        /* Roles for sink-inputs that are watched. */
        pa_hashmap *roles;

        /* Currently existing sink-inputs matching with roles.
         * key: sink-input-object data: uint32_t bit flag for sink-input
         * When sink-input is in playing state, si's flag is applied to enabled_slots
         * and vice-versa. */
        pa_hashmap *sink_inputs;
        uint32_t enabled_slots; /* bit slots that are enabled */
        uint32_t free_slots;    /* 1 means free, 0 means taken. */
    } notifier;
};

/* return either "media" or "call" volume steps struct based on whether
 * call is currently active */
struct mv_volume_steps* mv_active_steps(struct mv_userdata *u);

/* set new step as current step.
 * returns true if new step differs from current step.
 */
pa_bool_t mv_set_step(struct mv_userdata *u, unsigned step);

/* search for step with volume vol.
 * returns found step or -1 if not found
 */
int mv_search_step(int *steps, int n_steps, int vol);

/* update step based on information in volume proxy.
 * returns true if update was successfull.
 */
pa_bool_t mv_update_step(struct mv_userdata *u);

/* normalize mdB values to linear values */
void mv_normalize_steps(struct mv_volume_steps *steps);

/* parse step values to steps from step_string.
 * return number of steps found, or -1 on error
 */
int mv_parse_single_steps(struct mv_volume_steps *steps, const char *step_string);

/* parse step values for route from step_string_call and step_string_media.
 * after successfull parsing of both strings, new filled struct mv_volume_steps_set is
 * added to hashmap with route as key.
 * return total steps parsed or -1 on error.
 */
int mv_parse_steps(struct mv_userdata *u,
                   const char *route,
                   const char *step_string_call,
                   const char *step_string_media,
                   const char *high_volume); /* high_volume can be NULL */

/* Return highest step safe for listening with headphones. */
int mv_safe_step(struct mv_userdata *u);

/* Return true if current media step is same or over high volume step. */
pa_bool_t mv_high_volume(struct mv_userdata *u);

/* Return true if currently active media steps have high volume step defined. */
pa_bool_t mv_has_high_volume(struct mv_userdata *u);

#endif
