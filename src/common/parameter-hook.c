/*
 * This file is part of pulseaudio-meego
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
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

#include <pulsecore/hook-list.h>

#include "parameter-hook.h"

static pa_hook *parameter_update_requests = NULL;

int request_parameter_updates(const char *name, pa_hook_cb_t cb, pa_hook_priority_t prio, void *u) {
    struct connect_args args;

    if (!parameter_update_requests) {
        pa_log_warn("Parameter update service not available");
        return -1;
    }

    args.name = name;
    args.cb = cb;
    args.prio = prio;
    args.u = u;
    
    pa_log_warn("Requesting updates for %s", name);

    pa_hook_fire(parameter_update_requests, &args);

    return 0;
}

pa_hook_slot *receive_update_requests(pa_hook_cb_t cb, void *p) {
    if (!parameter_update_requests) {
        parameter_update_requests = pa_xnew0(pa_hook, 1);
        pa_hook_init(parameter_update_requests, p);
    }

    return pa_hook_connect(parameter_update_requests, PA_HOOK_NORMAL, cb, p);
}

void discontinue_update_requests(pa_hook_slot *s) {
    pa_hook_slot_free(s);
}

