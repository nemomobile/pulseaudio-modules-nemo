#ifndef __SRC_16_TO_48_H__
#define __SRC_16_TO_48_H__

/*
 * Copyright (C) 2010 Nokia Corporation.
 *
 * Contact: Maemo MMF Audio <mmf-audio@projects.maemo.org>
 *          or Jaska Uimonen  <jaska.uimonen@nokia.com>
 *
 * PulseAudio Meego modules are free software; you can redistribute
 * them and/or modify them under the terms of the GNU Lesser General Public
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

/* 320 frames equals 20ms */
#define SRC_16_TO_48_MAX_INPUT_FRAMES 320

static inline
int output_frames_src_16_to_48(int input_frames) {
    if (input_frames > SRC_16_TO_48_MAX_INPUT_FRAMES)
        return -1;
    return 3*input_frames;
}

struct src_16_to_48;
typedef struct src_16_to_48 src_16_to_48;

src_16_to_48 *alloc_src_16_to_48(void);

void free_src_16_to_48(src_16_to_48 *src);

int process_src_16_to_48(src_16_to_48 *src, short *output, short *input, int input_frames);

int process_src_16_to_48_mono_to_stereo(src_16_to_48 *src, short *output, short *input, int input_frames);

#endif /* __SRC_16_TO_48_H__ */
