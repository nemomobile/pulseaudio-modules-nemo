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
#ifndef pa_optimized_h
#define pa_optimized_h

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/memchunk.h>
#include <pulsecore/core.h>

int pa_optimized_take_channel(const pa_memchunk *ichunk, pa_memchunk *ochunk, int channel);

int pa_optimized_downmix_to_mono(const pa_memchunk *ichunk, pa_memchunk *ochunk);
int pa_optimized_equal_mix_in(pa_memchunk *ochunk, const pa_memchunk *ichunk);
int pa_optimized_mix_in_with_volume(pa_memchunk *ochunk, const pa_memchunk *ichunk, const pa_volume_t vol);
int pa_optimized_apply_volume(pa_memchunk *chunk, const pa_volume_t vol);
int pa_optimized_mono_to_stereo(const pa_memchunk *ichunk, pa_memchunk *ochunk);
int pa_optimized_interleave_stereo(const pa_memchunk *ichunk1, const pa_memchunk *ichunk2, pa_memchunk *ochunk);
int pa_optimized_deinterleave_stereo_to_mono(const pa_memchunk *ichunk, pa_memchunk *ochunk1, pa_memchunk *ochunk2);

#endif /* pa_optimized_h */
