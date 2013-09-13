/*
 * Copyright (C) 2013 Jolla Ltd.
 *
 * Contact: Juho Hämäläinen <juho.hamalainen@tieto.com>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/timeval.h>
#include <pulse/rtclock.h>
#include <pulse/xmalloc.h>

#include <pulsecore/core.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>

#include "listening-watchdog.h"

struct mv_listening_watchdog {
    pa_core *core;

    pa_bool_t initial_notify;
    pa_usec_t timeout;
    pa_usec_t start_time;
    pa_usec_t counter;

    pa_time_event *timer_event;

    mv_listening_watchdog_notify_cb_t notify_cb;
    void *userdata;
};

mv_listening_watchdog* mv_listening_watchdog_new(pa_core *core,
                                                 mv_listening_watchdog_notify_cb_t cb,
                                                 pa_usec_t timeout,
                                                 void *userdata) {
    mv_listening_watchdog *wd;

    pa_assert(core);
    pa_assert(cb);
    pa_assert(timeout > 0);

    wd = pa_xnew0(mv_listening_watchdog, 1);
    wd->core = core;
    wd->notify_cb = cb;
    wd->userdata = userdata;
    wd->timeout = timeout * PA_USEC_PER_SEC * 60;
    wd->initial_notify = TRUE;

    return wd;
}

void mv_listening_watchdog_free(mv_listening_watchdog *wd) {
    pa_assert(wd);

    mv_listening_watchdog_pause(wd);
    pa_xfree(wd);
}

static void timer_event_cb(pa_mainloop_api *a, pa_time_event *e, const struct timeval *t, void *userdata) {
    mv_listening_watchdog *wd = userdata;

    pa_assert(a);
    pa_assert(e);
    pa_assert(wd);

    pa_assert(e == wd->timer_event);
    mv_listening_watchdog_pause(wd);
    mv_listening_watchdog_reset(wd);

    pa_log_debug("Listening watchdog notify event.");
    wd->notify_cb(wd, FALSE, wd->userdata);
}

void mv_listening_watchdog_start(mv_listening_watchdog *wd) {
    pa_usec_t now, timer;

    pa_assert(wd);

    if (mv_listening_watchdog_running(wd))
        return;

    now = pa_rtclock_now();
    wd->start_time = now;
    timer = now + (wd->timeout - wd->counter);

    if (wd->initial_notify) {
        wd->notify_cb(wd, TRUE, wd->userdata);
        wd->initial_notify = FALSE;
    }

    wd->timer_event = pa_core_rttime_new(wd->core, timer, timer_event_cb, wd);
}

void mv_listening_watchdog_reset(mv_listening_watchdog *wd) {
    pa_bool_t running;

    pa_assert(wd);

    running = mv_listening_watchdog_running(wd);
    mv_listening_watchdog_pause(wd);
    wd->counter = 0;
    if (running)
        mv_listening_watchdog_start(wd);
}

void mv_listening_watchdog_pause(mv_listening_watchdog *wd) {
    pa_assert(wd);

    if (!mv_listening_watchdog_running(wd))
        return;

    wd->core->mainloop->time_free(wd->timer_event);
    wd->timer_event = NULL;
    wd->counter += pa_rtclock_now() - wd->start_time;
}

pa_bool_t mv_listening_watchdog_running(mv_listening_watchdog *wd) {
    pa_assert(wd);

    if (wd->timer_event)
        return TRUE;
    else
        return FALSE;
}
