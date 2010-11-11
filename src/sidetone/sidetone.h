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

#ifndef foosidetonehfoo
#define foosidetonehfoo

#include <pulsecore/core.h>

#define MAX_STEPS (64)

struct mv_volume_steps {
    int step[MAX_STEPS];
    int index[MAX_STEPS];
    int n_steps;
    int current_step;
};


typedef struct sidetone sidetone;


struct userdata {
    pa_module *module;
    sidetone *sidetone;
    char* argument;
    pa_hook_slot  *sidetone_parameters_updates;

};


sidetone *sidetone_new(pa_core *core, const char* argument);

void sidetone_free(sidetone *side);

#endif

