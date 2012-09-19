/***
  This file is part of PulseAudio.

  Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).

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

#include <pulsecore/core.h>
#include <pulsecore/hook-list.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/refcnt.h>
#include <pulsecore/shared.h>

#include "call-state-tracker.h"

struct pa_call_state_tracker {
    PA_REFCNT_DECLARE;

    pa_core *core;
    pa_bool_t active;
    pa_hook hooks[PA_CALL_STATE_HOOK_MAX];
};

static pa_call_state_tracker* call_state_tracker_new(pa_core *c) {
    pa_call_state_tracker *t;
    pa_call_state_hook_t h;

    pa_assert(c);

    t = pa_xnew0(pa_call_state_tracker, 1);
    PA_REFCNT_INIT(t);
    t->core = c;
    t->active = FALSE;

    for (h = 0; h < PA_CALL_STATE_HOOK_MAX; h++)
        pa_hook_init(&t->hooks[h], t);

    pa_assert_se(pa_shared_set(c, "call-state-tracker", t) >= 0);

    return t;
}

pa_call_state_tracker *pa_call_state_tracker_get(pa_core *core) {
    pa_call_state_tracker *t;

    if ((t = pa_shared_get(core, "call-state-tracker")))
        return pa_call_state_tracker_ref(t);

    return call_state_tracker_new(core);
}

pa_call_state_tracker *pa_call_state_tracker_ref(pa_call_state_tracker *t) {
    pa_assert(t);
    pa_assert(PA_REFCNT_VALUE(t) >= 1);

    PA_REFCNT_INC(t);

    return t;
}

void pa_call_state_tracker_unref(pa_call_state_tracker *t) {
    pa_call_state_hook_t h;

    pa_assert(t);
    pa_assert(PA_REFCNT_VALUE(t) >= 1);

    if (PA_REFCNT_DEC(t) > 0)
        return;

    for (h = 0; h < PA_CALL_STATE_HOOK_MAX; h++)
        pa_hook_done(&t->hooks[h]);

    pa_assert_se(pa_shared_remove(t->core, "call-state-tracker") >= 0);

    pa_xfree(t);
}

pa_bool_t pa_call_state_tracker_get_active(pa_call_state_tracker *t) {
    pa_assert(t);
    pa_assert(PA_REFCNT_VALUE(t) >= 1);

    return t->active;
}

void pa_call_state_tracker_set_active(pa_call_state_tracker *t, pa_bool_t active) {
    pa_bool_t changed;

    pa_assert(t);
    pa_assert(PA_REFCNT_VALUE(t) >= 1);

    changed = active != t->active;

    t->active = active;

    if (changed)
        pa_hook_fire(&t->hooks[PA_CALL_STATE_HOOK_CHANGED], (void *) active);

    pa_log_debug("Call state set %s (%s)", active ? "active" : "inactive", changed ? "changed" : "not changed");
}

pa_hook *pa_call_state_tracker_hooks(pa_call_state_tracker *t) {
    pa_assert(t);
    pa_assert(PA_REFCNT_VALUE(t) >= 1);

    return t->hooks;
}
