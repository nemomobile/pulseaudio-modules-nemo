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

#ifndef foosidetoneargshfoo
#define foosidetoneargshfoo

#include <asoundlib.h>

#include <pulsecore/modargs.h>

typedef struct {
    const char *mixer;

    const char **input_elements;
    snd_mixer_selem_channel_id_t *input_channels;
    int num_input_elements;

    const char **output_elements;
    snd_mixer_selem_channel_id_t *output_channels;
    int num_output_elements;

    const char *control_element;
    long target_volume;

    const char **sinks;
    int num_sinks;

    const char **sources;
    int num_sources;

    const char *sink_path;

    pa_modargs *modargs;
} sidetone_args;

sidetone_args* sidetone_args_new(const char* args);

void sidetone_args_free(sidetone_args *args);

#endif

