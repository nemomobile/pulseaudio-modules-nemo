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

#ifndef _module_meego_parameters_userdata_h_
#define _module_meego_parameters_userdata_h_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/core.h>
#include <pulsecore/modargs.h>

#include <meego/parameter-hook-implementor.h>
#include <meego/call-state-tracker.h>

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_modargs *modargs;

    pa_hook mode_hook;
    const char *mode;
    unsigned hash;

    struct parameters {
        const char *directory;
        pa_bool_t cache;
        pa_bool_t use_voice;
        PA_LLIST_HEAD(struct mode, modes); /* list of all modes */
        PA_LLIST_HEAD(struct algorithm, algorithms); /* list of all algorithms */
    } parameters;

    meego_parameter_hook_implementor_args implementor_args;

    pa_hook_slot *sink_proplist_changed_slot;
    pa_hook_slot *sink_input_move_finished_slot;

    pa_call_state_tracker *call_state_tracker;
};

#endif

