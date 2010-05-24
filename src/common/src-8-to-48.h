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
#ifndef __SRC_8_TO_48_H__
#define __SRC_8_TO_48_H__

/* 160 frames equals 20ms */
#define SRC_8_TO_48_MAX_INPUT_FRAMES 160

static inline
int output_frames_src_8_to_48(int input_frames) {
    if (input_frames > SRC_8_TO_48_MAX_INPUT_FRAMES)
	return -1;
    return 6*input_frames;
}

struct src_8_to_48;
typedef struct src_8_to_48 src_8_to_48;

src_8_to_48 *alloc_src_8_to_48(void);

void free_src_8_to_48(src_8_to_48 *src);

int process_src_8_to_48(src_8_to_48 *src, short *output, short *input, int input_frames);

int process_src_8_to_48_mono_to_stereo(src_8_to_48 *src, short *output, short *input, int input_frames);

#endif /* __SRC_8_TO_48_H__ */
