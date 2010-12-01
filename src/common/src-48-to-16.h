#ifndef __SRC_48_TO_16_H__
#define __SRC_48_TO_16_H__

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

#define SRC_48_TO_16_MAX_INPUT_FRAMES 960

static inline
int output_frames_src_48_to_16(int input_frames) {
    if ((input_frames%3) != 0 || input_frames > SRC_48_TO_16_MAX_INPUT_FRAMES)
        return -1;
    return input_frames/3;
}

static inline
int output_frames_src_48_to_16_total(int input_frames) {
    if ((input_frames%3) != 0)
        return -1;
    return input_frames/3;
}

struct src_48_to_16;
typedef struct src_48_to_16 src_48_to_16;

src_48_to_16 *alloc_src_48_to_16(void);

void free_src_48_to_16(src_48_to_16 *src);

int process_src_48_to_16(src_48_to_16 *src, short *output, short *input, int input_frames);

int process_src_48_to_16_stereo_to_mono(src_48_to_16 *src, short *output, short *input, int input_frames);

#endif /* __SRC_48_TO_16_H__ */
