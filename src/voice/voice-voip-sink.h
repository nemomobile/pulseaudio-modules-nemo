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
#ifndef voice_voip_sink_h
#define voice_voip_sink_h
#include <pulsecore/sink.h>

static inline
pa_bool_t voice_voip_sink_active(struct userdata *u) {
    pa_assert(u);
    return (u->voip_sink && u->voip_sink->state == PA_SINK_RUNNING);
}

static inline
pa_bool_t voice_voip_sink_active_iothread(struct userdata *u) {
    pa_assert(u);
    return (u->voip_sink && u->voip_sink->thread_info.state == PA_SINK_RUNNING);
}

int voice_init_voip_sink(struct userdata *u, const char *name);

#endif // voice_voip_sink_h
