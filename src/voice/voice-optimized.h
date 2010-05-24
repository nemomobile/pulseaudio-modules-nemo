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
#ifndef voice_optimized_h
#define voice_optimized_h

#include "module-voice-userdata.h"

typedef enum voice_channel
{
    VOICE_CH_0 = 0,
    VOICE_CH_1 = 1
} voice_channel;


/* int voice_take_channel1(struct userdata *u, const pa_memchunk *ichunk, pa_memchunk *ochunk); */
int voice_take_channel(struct userdata *u, const pa_memchunk *ichunk, pa_memchunk *ochunk, voice_channel ch);

int voice_downmix_to_mono(struct userdata *u, const pa_memchunk *ichunk, pa_memchunk *ochunk);
int voice_equal_mix_in(pa_memchunk *ochunk, const pa_memchunk *ichunk);
int voice_mix_in_with_volume(pa_memchunk *ochunk, const pa_memchunk *ichunk, const pa_volume_t vol);
int voice_apply_volume(pa_memchunk *chunk, const pa_volume_t vol);
int voice_mono_to_stereo(struct userdata *u, const pa_memchunk *ichunk, pa_memchunk *ochunk);
int voice_interleave_stereo(struct userdata *u, const pa_memchunk *ichunk1, const pa_memchunk *ichunk2, pa_memchunk *ochunk);
int voice_deinterleave_stereo_to_mono(struct userdata *u, const pa_memchunk *ichunk, pa_memchunk *ochunk1, pa_memchunk *ochunk2);


#endif // voice_optimized_h
