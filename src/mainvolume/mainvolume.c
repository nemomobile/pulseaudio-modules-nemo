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
#include <pulsecore/core-util.h>
#include <pulsecore/hashmap.h>
#include <pulsecore/call-state-tracker.h>
#include <pulsecore/volume-proxy.h>

#include "mainvolume.h"

struct mv_volume_steps* mv_active_steps(struct mv_userdata *u) {
    pa_assert(u);
    pa_assert(u->current_steps);

    if (u->call_active)
        return &u->current_steps->call;
    else
        return &u->current_steps->media;
}

pa_bool_t mv_set_step(struct mv_userdata *u, int step) {
    struct mv_volume_steps *s;
    pa_bool_t changed = FALSE;
    pa_assert(u);

    s = mv_active_steps(u);

    pa_assert(s);
    pa_assert(step < s->n_steps);

    if (s->current_step != step) {
        pa_log_debug("set current step to %d", step);
        s->current_step = step;

        if (u->call_active) {
            pa_volume_proxy_set_volume(u->volume_proxy, CALL_STREAM, s->step[s->current_step]);
        } else {
            pa_volume_proxy_set_volume(u->volume_proxy, MEDIA_STREAM, s->step[s->current_step]);
        }

        changed = TRUE;
    }

    return changed;
}

/* otherwise basic binary search except that exact value is not checked,
 * so that we can search by volume range.
 * returns found step or -1 if not found
 */
int mv_search_step(int *steps, int n_steps, int vol) {
    int sel = 0;

    int low = 0;
    int high = n_steps;
    int mid;

    while (low < high) {
        mid = low + ((high-low)/2);
        if (steps[mid] < vol)
            low = mid + 1;
        else
            high = mid;
    }

    /* check only that our search is valid, don't check
     * for exact value, so that we get step by range */
    if (low < n_steps)
        sel = low;
    else
        /* special case when volume is more than volume in last
         * step, we select the last ("loudest") step */
        sel = n_steps - 1;

    return sel;
}

pa_bool_t mv_update_step(struct mv_userdata *u) {
    pa_volume_t vol;
    pa_bool_t success = TRUE;
    int step;

    pa_assert(u);
    pa_assert(u->current_steps);

    if (!pa_volume_proxy_get_volume(u->volume_proxy, CALL_STREAM, &vol)) {
        step = mv_search_step(u->current_steps->call.step, u->current_steps->call.n_steps, vol);
        u->current_steps->call.current_step = step;
    } else
        success = FALSE;


    if (pa_volume_proxy_get_volume(u->volume_proxy, MEDIA_STREAM, &vol)) {
        step = mv_search_step(u->current_steps->media.step, u->current_steps->media.n_steps, vol);
        u->current_steps->media.current_step = step;
    } else
        success = FALSE;

    return success;
}

void mv_normalize_steps(struct mv_volume_steps *steps) {
    int i = 0;

    pa_assert(steps);
    pa_assert(steps->n_steps > 0);

    /* if first step is less than equal to -6000mB (consider
     * silent volume), set it directly to PA_VOLUME_MUTED */
    if (steps->step[0] <= -6000) {
        steps->step[0] = PA_VOLUME_MUTED;
        i = 1;
    }

    /* convert mB step values to software volume values.
     * divide mB values by 100.0 to get dB */
    for (; i < steps->n_steps; i++) {
        double value = (double)steps->step[i];
        steps->step[i] = pa_sw_volume_from_dB(value / 100.0);
    }
}

int mv_parse_single_steps(struct mv_volume_steps *steps, const char *step_string) {
    int len;
    int count = 0;
    int i = 0;

    pa_assert(steps);
    if (!step_string)
        return 0;

    len = strlen(step_string);

    while (i < len && count < MAX_STEPS) {
        char step[16];
        int value;
        size_t start, value_len;

        /* search for next step:value separator */
        for (; i < len && step_string[i] != ':'; i++);

        /* invalid syntax in step string, bail out */
        if (i == len)
            return -1;

        /* increment i by one to get to the start of value */
        i++;

        /* search for next step:value pair separator to determine value string length */
        start = i;
        for (; i < len && step_string[i] != ','; i++);
        value_len = i - start;

        if (value_len < 1 || value_len > sizeof(step)-1)
            return -1;

        /* copy value string part to step string and convert to integer */
        memcpy(step, &step_string[start], value_len);
        step[value_len] = '\0';

        if (pa_atoi(step, &value)) {
            return -1;
        }
        steps->step[count] = value;

        count++;
    }

    steps->n_steps = count;
    steps->current_step = 0;

    return count;
}

int mv_parse_steps(struct mv_userdata *u, const char *route, const char *step_string_call, const char *step_string_media) {
    int count1 = 0;
    int count2 = 0;
    struct mv_volume_steps_set *set;
    struct mv_volume_steps call_steps;
    struct mv_volume_steps media_steps;

    pa_assert(u);
    pa_assert(u->steps);
    pa_assert(route);

    if (!step_string_call || !step_string_media) {
        return 0;
    }

    count1 = mv_parse_single_steps(&call_steps, step_string_call);
    if (count1 < 1) {
        pa_log_warn("failed to parse call steps; %s", step_string_call);
        return -1;
    }
    mv_normalize_steps(&call_steps);

    count2 = mv_parse_single_steps(&media_steps, step_string_media);
    if (count2 < 1) {
        pa_log_warn("failed to parse media steps; %s", step_string_media);
        return -1;
    }
    mv_normalize_steps(&media_steps);

    set = pa_xnew0(struct mv_volume_steps_set, 1);
    set->route = pa_xstrdup(route);
    set->call = call_steps;
    set->media = media_steps;
    pa_log_debug("adding steps with route %s", set->route);
    pa_hashmap_put(u->steps, set->route, set);

    return count1 + count2;
}

