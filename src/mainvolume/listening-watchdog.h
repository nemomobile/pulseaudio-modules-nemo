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

#ifndef foolisteningwatchdoghfoo
#define foolisteningwatchdoghfoo

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulse/timeval.h>

#include <pulsecore/core.h>
#include <pulsecore/core-error.h>
#include <pulsecore/core-util.h>
#include <pulsecore/protocol-dbus.h>

typedef struct mv_listening_watchdog mv_listening_watchdog;

typedef void (*mv_listening_watchdog_notify_cb_t)(mv_listening_watchdog *watchdog, void *userdata);

/* Timeout in minutes. */
mv_listening_watchdog* mv_listening_watchdog_new(pa_core *core,
                                                 mv_listening_watchdog_notify_cb_t cb,
                                                 pa_usec_t timeout,
                                                 void *userdata);

void mv_listening_watchdog_free(mv_listening_watchdog *wd);

void mv_listening_watchdog_start(mv_listening_watchdog *wd);
void mv_listening_watchdog_reset(mv_listening_watchdog *wd);
void mv_listening_watchdog_pause(mv_listening_watchdog *wd);

pa_bool_t mv_listening_watchdog_running(mv_listening_watchdog *wd);

#endif
