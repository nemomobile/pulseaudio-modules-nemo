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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/core.h>
#include <pulsecore/hook-list.h>

#include "parameter-hook.h"
#include "parameter-hook-implementor.h"

static pa_hook parameter_update_requests;
static pa_hook *parameter_update_requests_ptr = NULL;

int meego_parameter_request_updates(const char *name, pa_hook_cb_t cb, pa_hook_priority_t prio, pa_bool_t full_updates, void *userdata) {
    meego_parameter_connection_args args;

    pa_assert(cb);

    if (!parameter_update_requests_ptr) {
        pa_log_warn("Parameter update service not available");
        return -1;
    }

    args.name = name;
    args.cb = cb;
    args.prio = prio;
    args.full_updates = full_updates;
    args.userdata = userdata;

    pa_log_debug("Requesting updates for %s", name ? name : "mode changes");

    pa_hook_fire(parameter_update_requests_ptr, &args);

    return 0;
}

pa_hook_slot* meego_parameter_receive_update_requests(pa_core *c, pa_hook_cb_t cb, void *userdata) {
    if (!parameter_update_requests_ptr) {
        parameter_update_requests_ptr = &parameter_update_requests;
        pa_hook_init(parameter_update_requests_ptr, c);
    }

    return pa_hook_connect(parameter_update_requests_ptr, PA_HOOK_NORMAL, cb, userdata);
}

void meego_parameter_discontinue_update_requests(pa_hook_slot *slot) {
    pa_hook_slot_free(slot);
    parameter_update_requests_ptr = NULL;
}
