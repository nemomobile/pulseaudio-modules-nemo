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
#ifndef _parameter_hook_h_
#define _parameter_hook_h_

typedef enum meego_parameter_status {
    MEEGO_PARAM_ENABLE,
    MEEGO_PARAM_DISABLE,
    MEEGO_PARAM_UPDATE,
    MEEGO_PARAM_MODE_CHANGE
} meego_parameter_status_t;

/*
 * Parameter updates are received in pa_hook_cb_t.
 * hook_data is pointer to pa_core.
 * call_data is pointer to meego_parameter_update_args explained below.
 * slot_data is pointer to userdata given in meego_parameter_request_updates().
 *
 * mode
 *      Null-terminated string of current audio mode.
 *
 * status MEEGO_PARAM_ENABLE:
 *      Parameters for given mode are already loaded, but mode
 *      changes to one with parameter.
 *      parameters and length are set to current parameter.
 *
 * status MEEGO_PARAM_DISABLE:
 *      Parameters are disabled for current mode, that is, no
 *      parameter are defined.
 *      parameters is NULL and length 0.
 *
 * status MEEGO_PARAM_UPDATE:
 *      Parameters for mode have changed from previous values. This
 *      also implies Enable.
 *      parameters and length are set to updated parameter.
 *
 * status MEEGO_PARAM_MODE_CHANGE:
 *      Mode has changed, but parameter state has not changed, that
 *      is, if status was disabled it is still so. This status
 *      is received only if full_updates was true in connection.
 *      parameters and length are set to currently active parameters.
 *      This status is also always set if only mode changes
 *      are requested. In that case parameters is NULL and length 0.
 *
 * Hook callback should always return PA_HOOK_OK.
 */
typedef struct meego_parameter_update_args {
    const char *mode;
    meego_parameter_status_t status;
    const void *parameters;
    unsigned length;
} meego_parameter_update_args;

/*
 * Request updates for parameters with given name. When full_updates is true, mode changes are sent also if
 * no changes in parameters (that is, callback is called always as mode changes).
 * If caller is only interested in mode updates, use NULL for name.
 * NOTE: The update requestor must call meego_parameter_stop_updates before being destroyed.
 */
int meego_parameter_request_updates(const char *name, pa_hook_cb_t cb, pa_hook_priority_t prio, bool full_updates, void *userdata);

/*
 * Stop calling the given callback "cb" with "userdata" for the algorithm
 * called "name". Every call to meego_parameter_request_updates must have a
 * corresponding meego_parameter_stop_updates call. cb and userdata must point
 * to the exact memory addresses that were passed to
 * meego_parameter_request_updates.
 */
int meego_parameter_stop_updates(const char *name, pa_hook_cb_t cb, void *userdata);

#endif
