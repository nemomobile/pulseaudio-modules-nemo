#ifndef __SRC_48_TO_16_H__
#define __SRC_48_TO_16_H__

/*

Copyright (C) 2008, 2009 Nokia Corporation.
This material, including documentation and any related
computer programs, is protected by copyright controlled by
Nokia Corporation. All rights are reserved. Copying,
including reproducing, storing,  adapting or translating, any
or all of this material requires the prior written consent of
Nokia Corporation. This material also contains confidential
information which may not be disclosed to others without the
prior written consent of Nokia Corporation.

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
