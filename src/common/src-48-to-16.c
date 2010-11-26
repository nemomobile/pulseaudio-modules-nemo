/*
 * Copyright (C) 2010 Nokia Corporation.
 *
 * Contact: Maemo MMF Audio <mmf-audio@projects.maemo.org>
 *          or Jaska Uimonen  <jaska.uimonen@nokia.com>
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

#include <stdio.h>

#include <string.h>
#include <stdlib.h>
#include "src-48-to-16.h"

#ifdef ARM_DSP
#include <dspfns.h>
#endif

#define FILTER_LENGTH 97
#define FILTER_MEMORY 96
#define FILTER_MEMORY_HOP 32

#define DOWNSAMPLING_FACTOR 3
#define STEREO_DOWNSAMPLING_FACTOR 6

struct src_48_to_16
{
  short filter_memory[FILTER_LENGTH * 2];
};

static const signed short filter_coeffs[] =
{
  0, 1, 6, 18, -12, -11, -10, 4, 18, 17, -2,
  -23, -27, -4, 29, 41, 14, -32, -57, -31,
  31, 76, 56, -23, -96, -90, 4, 113, 135,
  31, -122, -189, -89, 114, 250,
  177, -75, -306, -302, -21, 334,
  464, 217, -263, -625, -589, -188,
  247, 347, 28, -426, -590, -266,
  319, 680, 481, -156, -715, -703,
  -81, 668, 907, 388, -511, -1055,
  -750, 221, 1096, 1130, 220, -963,
  -1465, -808, 580, 1645, 1498, 132,
  -1501, -2157, -1225, 772, 2475, 2607,
  877, -1772, -3716, -3632, -1276, 2419,
  5920, 7925, 7960, 6431, 4236, 2232,
  883, 220
};

src_48_to_16 *alloc_src_48_to_16(void)
{
  src_48_to_16 *src = (src_48_to_16 *) malloc(sizeof(src_48_to_16));
  memset(src, 0, sizeof(*src));
  return src;
}

void free_src_48_to_16(src_48_to_16 *src)
{
  free(src);
}

#ifdef USE_SATURATION
#ifdef ARM_DSP
static inline int src_clip16(int input)
{
  return __ssat( input, 16 );
}
#else
static inline int src_clip16(int input)
{  input = input < (-32768) ? (-32768) : input;
  input = input > 32767 ?  32767 : input;
  return input;
}
#endif
#endif

int process_src_48_to_16(src_48_to_16 *s,
                        short *output,
                        short *input,
                        int input_frames)
{
    signed short *input_samples = 0;
    signed short *poly_start = 0;
    signed short *output_samples = 0;
    signed int result = 0;
    int output_frames = input_frames / 3;
    int samples_to_process = output_frames - FILTER_MEMORY_HOP;
    int start_point = 0;
    int i = 0;
    int j = 0;
    int k = 0;

    /* polyphase filter from 48kHz to 16kHz */

    /* first process the filter memory */
    start_point = 0;

    for (i = 0; i < FILTER_MEMORY_HOP; i++)
    {
      result = 0;

      for (j = start_point, k = 0; j < FILTER_MEMORY; j++, k++)
      {
        result += (int)(s->filter_memory[j]) * filter_coeffs[k];
      }

      for (j = 0; j < start_point + 1; j++, k++)
      {
        result += (int)(input[j]) * filter_coeffs[k];
      }

#ifdef USE_SATURATION
      output[i] = (short)src_clip16((result + 16384) >> 15);
#else
      output[i] = (short)((result + 16384) >> 15);
#endif

      start_point += DOWNSAMPLING_FACTOR;
    }

    output_samples = &output[FILTER_MEMORY_HOP];
    poly_start = &input[0];

    /* then process the rest of the input buffer */
    for (i = 0; i < samples_to_process; i++)
    {
      result = 0;
      input_samples = poly_start;

      for (j = 0; j < FILTER_LENGTH; j++)
      {
        result += (int)(*input_samples++) * filter_coeffs[j];
      }

#ifdef USE_SATURATION
      *output_samples++ = (short)src_clip16((result + 16384) >> 15);
#else
      *output_samples++ = (short)((result + 16384) >> 15);
#endif

      poly_start += DOWNSAMPLING_FACTOR;
    }

    /* copy filter memory to the beginning of scratch buffer */
    memcpy((void*)(&(s->filter_memory[0])),
           (void*)poly_start,
           FILTER_MEMORY * sizeof(short));


    return output_frames;
}

int process_src_48_to_16_stereo_to_mono(src_48_to_16 *s,
                                       short *output,
                                       short *input,
                                       int input_frames)
{
    signed short *input_samples = 0;
    signed short *poly_start = 0;
    signed short *output_samples = 0;
    signed int result = 0;
    int output_frames = input_frames / 6;
    int samples_to_process = output_frames - FILTER_MEMORY_HOP;
    int start_point = 0;
    int i = 0;
    int j = 0;
    int k = 0;

    /* polyphase filter from 48kHz to 16kHz */

    /* first process the filter memory */
    start_point = 0;

    for (i = 0; i < FILTER_MEMORY_HOP; i++)
    {
      result = 0;

      for (j = start_point, k = 0; j < FILTER_MEMORY * 2; j += 2, k++)
      {
        result += (int)(s->filter_memory[j]) * filter_coeffs[k];
      }

      /*
      printf("1. j = %d\n", j);
      printf("1. k = %d\n", k);
      */

      for (j = 0; j < start_point + 2; j += 2, k++)
      {
        result += (int)(input[j]) * filter_coeffs[k];
      }

      /*
      printf("2. j = %d\n", j);
      printf("2. k = %d\n", k);
      printf("\n");
      */

#ifdef USE_SATURATION
      output[i] = (short)src_clip16((result + 16384) >> 15);
#else
      output[i] = (short)((result + 16384) >> 15);
#endif

      start_point += STEREO_DOWNSAMPLING_FACTOR;
    }

    output_samples = &output[FILTER_MEMORY_HOP];
    poly_start = &input[0];

    /* then process the rest of the input buffer */
    for (i = 0; i < samples_to_process; i++)
    {
      result = 0;
      input_samples = poly_start;

      for (j = 0; j < FILTER_LENGTH; j++)
      {
        result += (int)(*input_samples) * filter_coeffs[j];
        input_samples += 2;
      }

#ifdef USE_SATURATION
      *output_samples++ = (short)src_clip16((result + 16384) >> 15);
#else
      *output_samples++ = (short)((result + 16384) >> 15);
#endif

      poly_start += STEREO_DOWNSAMPLING_FACTOR;
    }

    /* copy rest of input to filter memory  */
    memcpy((void*)(&(s->filter_memory[0])),
           (void*)poly_start,
           2 * FILTER_MEMORY * sizeof(short));

    return output_frames;
}
