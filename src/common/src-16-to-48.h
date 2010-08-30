#ifndef __SRC_16_TO_48_H__
#define __SRC_16_TO_48_H__

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
