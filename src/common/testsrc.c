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

#include <stdio.h>
#include "src-48-to-8.h"
#include "src-8-to-48.h"

void usage();

int main(int argc, char *argv[])
{
    FILE *fp_in = 0;
    FILE *fp_out = 0;
    short input_buffer[960];
    short output_buffer[960];
    int samples_read = 0;
    int samples_processed = 0;
    int samples_in = 0;
    int flag = 0;
    src_8_to_48 *src8to48;
    src_48_to_8 *src48to8;

    if (argc != 4)
    {
      usage();
      return 0;
    }

    if ((fp_in = fopen(argv[1], "rb")) == NULL)
    {
        printf("can't open input file\n");
        return 0;
    }

    if ((fp_out = fopen(argv[2], "wb")) == NULL)
    {
        printf("can't open output file\n");
        return 0;
    }

    sscanf(argv[3], "%d", &flag);

    if (flag > 3 || flag < 0)
    {
      usage();
      return 0;
    }

    src8to48 = alloc_src_8_to_48();
    src48to8 = alloc_src_48_to_8();

    switch (flag)
    {
    case 0:
      {
	samples_in = 480;
	while((samples_read = fread(input_buffer, sizeof(short), samples_in, fp_in)) == samples_in)
	{
	  samples_processed = process_src_48_to_8(src48to8,
						  output_buffer,
						  input_buffer,
						  samples_in);

	  fwrite(output_buffer, sizeof(short), samples_processed, fp_out);

	}

	break;
      }
    case 1:
      {
	samples_in = 960;
	while((samples_read = fread(input_buffer, sizeof(short), samples_in, fp_in)) == samples_in)
	{
	  samples_processed = process_src_48_to_8_stereo_to_mono(src48to8,
								 output_buffer,
								 input_buffer,
								 samples_in);

	  fwrite(output_buffer, sizeof(short), samples_processed, fp_out);

	}

	break;
      }
    case 2:
      {
	samples_in = 80;
	while((samples_read = fread(input_buffer, sizeof(short), samples_in, fp_in)) == samples_in)
	{
	  samples_processed = process_src_8_to_48(src8to48,
						  output_buffer,
						  input_buffer,
						  samples_in);

	  fwrite(output_buffer, sizeof(short), samples_processed, fp_out);
	}

	break;
      }
    case 3:
      {
	samples_in = 80;
	while((samples_read = fread(input_buffer, sizeof(short), samples_in, fp_in)) == samples_in)
	{
	  samples_processed = process_src_8_to_48_mono_to_stereo(src8to48,
								 output_buffer,
								 input_buffer,
								 samples_in);

	  fwrite(output_buffer, sizeof(short), samples_processed, fp_out);
	}
	break;
      }
    default:
      printf("something went totally wrong!\n");
    }

    free_src_8_to_48(src8to48);
    free_src_48_to_8(src48to8);

    fclose(fp_in);
    fclose(fp_out);

    return 0;
}

void usage()
{
  printf("usage:test input_file output_file flag\n");
  printf("flag:\n");
  printf("0 - 48kHz mono to 8kHz mono\n");
  printf("1 - 48kHz stereo to 8kHz mono\n");
  printf("2 - 8kHz mono to 48kHz mono\n");
  printf("3 - 8kHz mono to 48kHz stereo\n");
}
