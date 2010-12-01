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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "src-16-to-48.h"

#ifdef ARM_DSP
#include <dspfns.h>
#endif

#define FILTER_LENGTH 32
#define FILTER_MEMORY 31

struct src_16_to_48 {
    short filter_memory[FILTER_LENGTH];
};

static const signed short filter_coeffs_A[] = {
53, -29, 51, -81, 122, -172, 229, -288, 340,
-367, 343, -224, -64, 651, -1768, 1042, -1770,
2040, -2144, 2003, -1533, 663, 660, -2424,
4494, -6472, 7424, -5316, -3829, 23775,
12709, 659
};

static const signed short filter_coeffs_B[] = {
19, -34, 53, -70, 86, -96, 94, -70, 12, 94,
-268, 531, -905, 1393, -1875, 741, -1279, 956,
-467, -243, 1165, -2249, 3391, -4394, 4936,
-4502, 2317, 2632, -10896, 17759, 19294, 2649
};

static const signed short filter_coeffs_C[] = {
2, -35, 13, -5, -12, 43, -94, 168, -271, 405,
-568, 749, -919, 1003, -789, -563, 85, -799,
1443, -2110, 2722, -3166, 3287, -2890, 1741,
397, -3674, 7822, -11148, 7258, 23879, 6696
};

src_16_to_48 *alloc_src_16_to_48(void)
{
    src_16_to_48 *src = (src_16_to_48 *) malloc(sizeof(src_16_to_48));
    memset(src, 0, sizeof(*src));
    return src;
}

void free_src_16_to_48(src_16_to_48 *src)
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
{
  input = input < (-32768) ? (-32768) : input;
  input = input > 32767 ?  32767 : input;
  return input;
}
#endif
#endif

int process_src_16_to_48(src_16_to_48 *s,
                        short *output,
                        short *input,
                        int input_frames)
{
    signed short *input_samples = 0;
    signed short *output_samples = 0;
    signed short *poly_start = 0;
    int output_frames = 3 * input_frames;
    int samples_to_process = input_frames - FILTER_MEMORY;
    signed int result_A = 0;
    signed int result_B = 0;
    signed int result_C = 0;
    int i = 0;
    int j = 0;
    int k = 0;

    /* low pass filtering */

    output_samples = &output[0];

    /* first process the filter memory */
    for (i = 0; i < FILTER_MEMORY; i++)
    {
      result_A = 0;
      result_B = 0;
      result_C = 0;

      for (j = i, k = 0; j < FILTER_MEMORY; j++, k++)
      {
        result_A += (int)s->filter_memory[j] * filter_coeffs_A[k];
        result_B += (int)s->filter_memory[j] * filter_coeffs_B[k];
        result_C += (int)s->filter_memory[j] * filter_coeffs_C[k];
      }

      /*
      printf("1. j = %d\n", j);
      printf("1. k = %d\n", k);
      */

      for (j = 0; j < i + 1; j++, k++)
      {
        result_A += (int)input[j] * filter_coeffs_A[k];
        result_B += (int)input[j] * filter_coeffs_B[k];
        result_C += (int)input[j] * filter_coeffs_C[k];
      }

      /*
      printf("2. j = %d\n", j);
      printf("2. k = %d\n", k);
      printf("\n");
      */

#ifdef USE_SATURATION
      *output_samples++ = (short)src_clip16((result_A + 16384) >> 15);
      *output_samples++ = (short)src_clip16((result_B + 16384) >> 15);
      *output_samples++ = (short)src_clip16((result_C + 16384) >> 15);
#else
      *output_samples++ = (short)((result_A + 16384) >> 15);
      *output_samples++ = (short)((result_B + 16384) >> 15);
      *output_samples++ = (short)((result_C + 16384) >> 15);
#endif
    }

    poly_start = &input[0];

    /* then process rest of the input */
    for (i = 0; i < samples_to_process; i++)
    {
      result_A = 0;
      result_B = 0;
      result_C = 0;
      input_samples = poly_start;

      for (j = 0; j < FILTER_LENGTH; j++)
      {
        result_A += (int)(*input_samples) * filter_coeffs_A[j];
        result_B += (int)(*input_samples) * filter_coeffs_B[j];
        result_C += (int)(*input_samples++) * filter_coeffs_C[j];
      }

#ifdef USE_SATURATION
      *output_samples++ = (short)src_clip16((result_A + 16384) >> 15);
      *output_samples++ = (short)src_clip16((result_B + 16384) >> 15);
      *output_samples++ = (short)src_clip16((result_C + 16384) >> 15);
#else
      *output_samples++ = (short)((result_A + 16384) >> 15);
      *output_samples++ = (short)((result_B + 16384) >> 15);
      *output_samples++ = (short)((result_C + 16384) >> 15);
#endif
      poly_start++;
    }

    /* copy unused samples to filter_memory */
    memcpy((void*)(&(s->filter_memory[0])),
           (void*)(&(input[samples_to_process])),
           FILTER_MEMORY * sizeof(short));

    return output_frames;
}

int process_src_16_to_48_mono_to_stereo(src_16_to_48 *s,
                                       short *output,
                                       short *input,
                                       int input_frames)

{
    signed short *input_samples = NULL;
    signed short *output_samples = NULL, *prev_output_samples = NULL;
    signed short *poly_start = NULL;
    int output_frames = 6 * input_frames;
    int samples_to_process = input_frames - FILTER_MEMORY;
    signed int result_A = 0;
    signed int result_B = 0;
    signed int result_C = 0;
    int i = 0;
    int j = 0;
    int k = 0;

    output_samples = &output[0];

    /* first process the filter memory */
    for (i = 0; i < FILTER_MEMORY; i++)
    {
      result_A = 0;
      result_B = 0;
      result_C = 0;

      for (j = i, k = 0; j < FILTER_MEMORY; j++, k++)
      {
        result_A += (int)s->filter_memory[j] * filter_coeffs_A[k];
        result_B += (int)s->filter_memory[j] * filter_coeffs_B[k];
        result_C += (int)s->filter_memory[j] * filter_coeffs_C[k];
      }

      for (j = 0; j < i + 1; j++, k++)
      {
        result_A += (int)input[j] * filter_coeffs_A[k];
        result_B += (int)input[j] * filter_coeffs_B[k];
        result_C += (int)input[j] * filter_coeffs_C[k];
      }

#ifdef USE_SATURATION
      prev_output_samples = output_samples;
      *output_samples++ = (short)src_clip16((result_A + 16384) >> 15);
      *output_samples++ = *prev_output_samples;

      prev_output_samples = output_samples;
      *output_samples++ = (short)src_clip16((result_B + 16384) >> 15);
      *output_samples++ = *prev_output_samples;

      prev_output_samples = output_samples;
      *output_samples++ = (short)src_clip16((result_C + 16384) >> 15);
      *output_samples++ = *prev_output_samples;
#else
      prev_output_samples = output_samples;
      *output_samples++ = (short)((result_A + 16384) >> 15);
      *output_samples++ = *prev_output_samples;

      prev_output_samples = output_samples;
      *output_samples++ = (short)((result_B + 16384) >> 15);
      *output_samples++ = *prev_output_samples;

      prev_output_samples = output_samples;
      *output_samples++ = (short)((result_C + 16384) >> 15);
      *output_samples++ = *prev_output_samples;
#endif
    }

    poly_start = &input[0];

    /* then process rest of the input */
    for (i = 0; i < samples_to_process; i++)
    {
      result_A = 0;
      result_B = 0;
      result_C = 0;
      input_samples = poly_start;

      for (j = 0; j < FILTER_LENGTH; j++)
      {
        result_A += (int)(*input_samples) * filter_coeffs_A[j];
        result_B += (int)(*input_samples) * filter_coeffs_B[j];
        result_C += (int)(*input_samples++) * filter_coeffs_C[j];
      }

#ifdef USE_SATURATION
      prev_output_samples = output_samples;
      *output_samples++ = (short)src_clip16((result_A + 16384) >> 15);
      *output_samples++ = *prev_output_samples;

      prev_output_samples = output_samples;
      *output_samples++ = (short)src_clip16((result_B + 16384) >> 15);
      *output_samples++ = *prev_output_samples;

      prev_output_samples = output_samples;
      *output_samples++ = (short)src_clip16((result_C + 16384) >> 15);
      *output_samples++ = *prev_output_samples;
#else
      prev_output_samples = output_samples;
      *output_samples++ = (short)((result_A + 16384) >> 15);
      *output_samples++ = *prev_output_samples;

      prev_output_samples = output_samples;
      *output_samples++ = (short)((result_B + 16384) >> 15);
      *output_samples++ = *prev_output_samples;

      prev_output_samples = output_samples;
      *output_samples++ = (short)((result_C + 16384) >> 15);
      *output_samples++ = *prev_output_samples;
#endif
      poly_start++;
    }

    /* copy unused samples to filter_memory */
    memcpy((void*)(&(s->filter_memory[0])),
           (void*)(&(input[samples_to_process])),
           FILTER_MEMORY * sizeof(short));

    return output_frames;
}

