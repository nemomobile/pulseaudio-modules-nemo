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
/* TODO: those functions are exported and should have a namespace, such as meego_rabbit_ */

#ifndef OPTIMIZE_H_
#define OPTIMIZE_H_

#include <stdint.h>


void move_16bit_to_32bit(int32_t *dst, const short *src, unsigned n);
void move_32bit_to_16bit(short *dst, const int32_t *src, unsigned n);

void interleave_mono_to_stereo(const short *src[], short *dst, unsigned n);
void deinterleave_stereo_to_mono(const short *src, short *dst[], unsigned n);

/**
* Extracts a single mono channel from interleaved stereo input.
* \param[in] src stereo audio data
* \param[out] dst mono audio data
* \param[in] n input data length in samples
* \param[in] ch channel to extract <b>NOTE: must be either 0 or 1</b>
*/
void extract_mono_from_interleaved_stereo(const short *src, short *dst, unsigned n, unsigned ch);
void downmix_to_mono_from_interleaved_stereo(const short *src, short *dst, unsigned n);
void dup_mono_to_interleaved_stereo(const short *src, short *dst, unsigned n);
void symmetric_mix(const short *src1, const short *src2, short *dst, const unsigned n);
void mix_in_with_volume(const short volume, const short *src, short *dst, const unsigned n);
void apply_volume(const short volume, const short *src, short *dst, const unsigned n);

#endif
