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
#ifndef _parameter_hook_implementor_h_
#define _parameter_hook_implementor_h_

#include "parameter-hook.h"

typedef struct meego_parameter_connection_args {
    const char *name;
    pa_hook_cb_t cb;
    pa_hook_priority_t prio;
    pa_bool_t full_updates;
    void *userdata;
} meego_parameter_connection_args;

pa_hook_slot* meego_parameter_receive_update_requests(pa_core *c, pa_hook_cb_t cb, void *userdata);
void meego_parameter_discontinue_update_requests(pa_hook_slot *slot);


#endif
