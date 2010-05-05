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

#ifndef foomainvolumehfoo
#define foomainvolumehfoo

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/core.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/call-state-tracker.h>
#include <pulsecore/volume-proxy.h>
#include <pulsecore/protocol-dbus.h>

#define MEDIA_STREAM "sink-input-by-media-role:x-maemo"
#define CALL_STREAM "sink-input-by-media-role:phone"
#define MAX_STEPS (64)

struct mv_volume_steps {
    int step[MAX_STEPS];
    int n_steps;
    int current_step;
};

struct mv_volume_steps_set {
    char *route;

    struct mv_volume_steps call;
    struct mv_volume_steps media;
};

struct mv_userdata {
    pa_core *core;
    pa_module *module;
    pa_bool_t tuning_mode;

    pa_hashmap *steps;
    struct mv_volume_steps_set *current_steps;
    char *route;

    pa_call_state_tracker *call_state_tracker;
    pa_hook_slot *call_state_tracker_slot;
    pa_bool_t call_active;

    pa_volume_proxy *volume_proxy;
    pa_hook_slot *volume_proxy_slot;

    pa_hook_slot *sink_proplist_changed_slot;

    pa_dbus_protocol *dbus_protocol;
    char *dbus_path;
};

/* return either "media" or "call" volume steps struct based on whether
 * call is currently active */
struct mv_volume_steps* mv_active_steps(struct mv_userdata *u);

/* set new step as current step.
 * returns true if new step differs from current step.
 */
pa_bool_t mv_set_step(struct mv_userdata *u, int step);

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
int mv_parse_steps(struct mv_userdata *u, const char *route, const char *step_string_call, const char *step_string_media);

#endif
