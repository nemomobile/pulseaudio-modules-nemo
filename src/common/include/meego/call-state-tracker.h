#ifndef foocallstatetrackerhfoo
#define foocallstatetrackerhfoo

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

/* This is a silly little shared singleton object used by module-nokia-voice
 * and module-policy-enforcement. The purpose of the object is just to maintain
 * a boolean state of "call is active" or "call is not active", and to provide
 * notification hooks for tracking state changes. module-nokia-voice takes care
 * of setting the correct state whenever it changes, and
 * module-policy-enforcement listens to those state changes so that it can take
 * the state into account in its own operation.
 *
 * FIXME: It would probably make more sense to implement a generic shared state
 * system, which would do pretty much the same thing as pa_core.shared, except
 * that value changes would fire hooks. Or maybe pa_core should have its own
 * proplist and define a new hook PA_CORE_HOOK_PROPLIST_CHANGED... */

#include <pulsecore/core.h>
#include <pulsecore/hook-list.h>

typedef struct pa_call_state_tracker pa_call_state_tracker;

/* Hook data: pa_call_state_tracker pointer. */
typedef enum pa_call_state_hook {
    PA_CALL_STATE_HOOK_CHANGED, /* Call data: NULL. */
    PA_CALL_STATE_HOOK_MAX
} pa_call_state_hook_t;

pa_call_state_tracker *pa_call_state_tracker_get(pa_core *core);
pa_call_state_tracker *pa_call_state_tracker_ref(pa_call_state_tracker *t);
void pa_call_state_tracker_unref(pa_call_state_tracker *t);

/* If the value has not been explicitly set, returns false. */
bool pa_call_state_tracker_get_active(pa_call_state_tracker *t);

void pa_call_state_tracker_set_active(pa_call_state_tracker *t, bool active);

pa_hook *pa_call_state_tracker_hooks(pa_call_state_tracker *t);

#endif
