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
#ifndef voice_hw_sink_process_h
#define voice_hw_sink_process_h

#include "module-voice-userdata.h"

#include "src/common/optimized.h"
#include "voice-optimized.h"

static
void voice_hw_sink_process(struct userdata *u, pa_memchunk *chunk)
{
    pa_assert(u);
    if (chunk->length % u->hw_fragment_size) {
	pa_log_debug("chunk->length = %d ", chunk->length);
    }
    pa_assert(0 == (chunk->length % u->hw_fragment_size));
    pa_assert(u->sink_temp_buff_len >= chunk->length);

    pa_hook_fire(u->hooks[HOOK_HW_SINK_PROCESS], chunk);

}

static
void voice_hw_sink_process_nb_eeq_mono(struct userdata *u, pa_memchunk *chunk)
{
    pa_assert(u);
    pa_assert(0 == (chunk->length % (u->aep_fragment_size/2)));

    pa_hook_fire(u->hooks[HOOK_NARROWBAND_EAR_EQU_MONO], chunk);
}

static
void voice_hw_sink_process_xprot_mono_to_stereo(struct userdata *u, pa_memchunk *chunk)
{
    pa_assert(u);
    pa_assert(0 == (chunk->length % u->hw_mono_fragment_size));
    pa_bool_t converted_to_stereo = 0;

    pa_hook_fire(u->hooks[HOOK_XPROT_MONO], chunk);

    if (!converted_to_stereo) {
	pa_memchunk ochunk;
	voice_mono_to_stereo(u, chunk, &ochunk);
	pa_memblock_unref(chunk->memblock);
	*chunk = ochunk;
    }
}

#endif
