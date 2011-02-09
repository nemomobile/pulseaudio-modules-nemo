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
#ifndef __SRC_48_TO_8_H__
#define __SRC_48_TO_8_H__

#define SRC_48_TO_8_MAX_INPUT_FRAMES 960

static inline
int output_frames_src_48_to_8(int input_frames) {
    if ((input_frames%6) != 0 || input_frames > SRC_48_TO_8_MAX_INPUT_FRAMES)
	return -1;
    return input_frames/6;
}

static inline
int output_frames_src_48_to_8_total(int input_frames) {
    if ((input_frames%6) != 0)
	return -1;
    return input_frames/6;
}

struct src_48_to_8;
typedef struct src_48_to_8 src_48_to_8;

src_48_to_8 *alloc_src_48_to_8(void);

void free_src_48_to_8(src_48_to_8 *src);

int process_src_48_to_8(src_48_to_8 *src, short *output, short *input, int input_frames);

int process_src_48_to_8_stereo_to_mono(src_48_to_8 *src, short *output, short *input, int input_frames);

#endif /* __SRC_48_TO_8_H__ */
