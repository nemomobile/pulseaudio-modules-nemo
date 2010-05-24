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
#ifndef voice_voip_source_h
#define voice_voip_source_h

#include <pulsecore/source.h>

static inline
pa_bool_t voice_voip_source_active(struct userdata *u) {
    return (u->voip_source && (u->voip_source->state == PA_SOURCE_RUNNING ||
                               u->voip_source->state == PA_SOURCE_IDLE));
}

static inline
pa_bool_t voice_voip_source_active_iothread(struct userdata *u) {
    return (u->voip_source && (u->voip_source->thread_info.state == PA_SOURCE_RUNNING));
}

int voice_init_voip_source(struct userdata *u, const char *name);

#endif // voice_voip_source_h
