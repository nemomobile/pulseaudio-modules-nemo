/*
 * This file is part of pulseaudio-meego
 *
 * Copyright (C) 2008, 2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Maemo Multimedia <multimedia@maemo.org>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved.
 *
 * Copying, including reproducing, storing, adapting or translating,
 * any or all of this material requires the prior written consent of
 * Nokia Corporation. This material also contains confidential
 * information which may not be disclosed to others without the prior
 * written consent of Nokia.
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
