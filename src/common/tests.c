#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "optimized.h"

#define TEST_LENGTH 160

int test_extract(int argc, char *argv[])
{
  int i = 0;
  short test_input[TEST_LENGTH];
  short test_result[TEST_LENGTH / 2];

  printf("\n * Test: %s\n", __PRETTY_FUNCTION__);

  for (i = 0; i < TEST_LENGTH; i++)
    {
      test_input[i] = 1;
      i++;
      test_input[i] = 2;
    }

  for (i = 0; i < TEST_LENGTH / 2; i++)
    {
      test_result[i] = 0;
    }

  extract_mono_from_interleaved_stereo(test_input, &test_result[0], TEST_LENGTH);

  for (i = 0; i < TEST_LENGTH / 2; i++)
    {
      printf("test result in index %d is %d\n", i, test_result[i]);
    }

  return 0;
}

int test_interleave(int argc, char *argv[])
{
  int i = 0;
  short test_input_1[TEST_LENGTH / 2];
  short test_input_2[TEST_LENGTH / 2];
  const short *test_input[2];
  short test_result[TEST_LENGTH];

  printf("\n * Test: %s\n", __PRETTY_FUNCTION__);

  test_input[0] = &(test_input_1[0]);
  test_input[1] = &(test_input_2[0]);

  for (i = 0; i < TEST_LENGTH / 2; i++)
  {
    test_input_1[i] = 1;
    test_input_2[i] = 2;
  }

  for (i = 0; i < TEST_LENGTH; i++)
  {
    test_result[i] = 0;
  }

  interleave_mono_to_stereo(test_input, &test_result[0], TEST_LENGTH / 2);

  for (i = 0; i < TEST_LENGTH; i++)
  {
    printf("test result in index %d is %d\n", i, test_result[i]);
  }

  return 0;
}

int test_deinterleave(int argc, char *argv[])
{
  int i = 0;
  short test_input[TEST_LENGTH];
  short test_result_1[TEST_LENGTH / 2];
  short test_result_2[TEST_LENGTH / 2];
  short *test_result[2];

  printf("\n * Test: %s\n", __PRETTY_FUNCTION__);

  test_result[0] = test_result_1;
  test_result[1] = test_result_2;

  for (i = 0; i < TEST_LENGTH; i++)
    {
      test_input[i] = 1;
      i++;
      test_input[i] = 2;
    }

  for (i = 0; i < TEST_LENGTH / 2; i++)
    {
      test_result_1[i] = 0;
      test_result_2[i] = 0;
    }

  deinterleave_stereo_to_mono((const short *)(&test_input[0]), test_result, TEST_LENGTH);

  for (i = 0; i < TEST_LENGTH / 2; i++)
    {
      printf("test result 1 in idex %d is %d\n", i, test_result_1[i]);
    }

  for (i = 0; i < TEST_LENGTH / 2; i++)
    {
      printf("test result 2 in index %d is %d\n", i, test_result_2[i]);
    }

  return 0;
}

int test_downmix(int argc, char *argv[])
{
  int i = 0;
  short test_input[TEST_LENGTH];
  short test_result[TEST_LENGTH / 2];

  printf("\n * Test: %s\n", __PRETTY_FUNCTION__);

  for (i = 0; i < TEST_LENGTH; i++)
    {
      test_input[i] = 200*(i-(TEST_LENGTH/2));
      i++;
      test_input[i] = 200*i;
    }

  for (i = 0; i < TEST_LENGTH / 2; i++)
    {
      test_result[i] = 0;
    }

  downmix_to_mono_from_interleaved_stereo((const short *)(&test_input[0]), test_result, TEST_LENGTH);

  for (i = 0; i < TEST_LENGTH / 2; i++)
    {
	printf("test result %d: %d + %d = %d\n", i, test_input[2*i], test_input[2*i+1], test_result[i]);
    }

  return 0;
}

int test_dup(int argc, char *argv[])
{
  int i = 0;
  short test_input[TEST_LENGTH / 2];
  short test_result[TEST_LENGTH];

  printf("\n * Test: %s\n", __PRETTY_FUNCTION__);

  for (i = 0; i < TEST_LENGTH / 2; i++)
    {
      test_input[i] = 1;
      i++;
      test_input[i] = 2;
    }

  for (i = 0; i < TEST_LENGTH; i++)
    {
      test_result[i] = 0;
    }

  dup_mono_to_interleaved_stereo(test_input, test_result, TEST_LENGTH / 2);

  for (i = 0; i < TEST_LENGTH; i++)
    {
      printf("test result in index %d is %d\n", i, test_result[i]);
    }

  return 0;
}

int test_mix(int argc, char *argv[])
{
  int i = 0;
  short test_input1[TEST_LENGTH];
  short test_input2[TEST_LENGTH];
  short test_result[TEST_LENGTH];

  printf("\n * Test: %s\n", __PRETTY_FUNCTION__);

  for (i = 0; i < TEST_LENGTH; i++)
    {
       test_input1[i] = ((i%61)-30)*1000;
       test_input2[i] = ((i%31)-15)*2000;
    }

  for (i = 0; i < TEST_LENGTH; i++)
    {
      test_result[i] = 0;
    }

  symmetric_mix(test_input1, test_input2, test_result, TEST_LENGTH);

  for (i = 0; i < TEST_LENGTH; i++)
    {
	printf("test result %d + %d = %d (sample %d)\n",
	       test_input1[i], test_input2[i], test_result[i], i);
    }

  return 0;
}

int test_mix_in_with_volume(int argc, char *argv[])
{
  int i = 0;
  short test_input1[TEST_LENGTH];
  short test_input2[TEST_LENGTH];
  short test_result[TEST_LENGTH];
  short volume = INT16_MAX;
  printf("\n * Test: %s\n", __PRETTY_FUNCTION__);

  if (argc >= 2)
      volume = (short)atoi(argv[1]);

  printf("volume = %d\n", (int)volume);

  for (i = 0; i < TEST_LENGTH; i++)
    {
      test_result[i] = test_input1[i] = ((i%61)-30)*1000;
      test_input2[i] = ((i%31)-15)*2000;
    }

  mix_in_with_volume(volume, test_input2, test_result, TEST_LENGTH);

  for (i = 0; i < TEST_LENGTH; i++)
    {
	printf("test result %d + %d*%d = %d (sample %d)\n",
	       test_input1[i], test_input2[i], volume, test_result[i], i);
    }

  return 0;
}

int test_apply_volume(int argc, char *argv[])
{
  int i = 0;
  short test_input1[TEST_LENGTH];
  short test_result[TEST_LENGTH];
  short volume = INT16_MAX;
  printf("\n * Test: %s\n", __PRETTY_FUNCTION__);

  if (argc >= 2)
      volume = (short)atoi(argv[1]);

  printf("volume = %d\n", (int)volume);

  for (i = 0; i < TEST_LENGTH; i++)
    {
      test_input1[i] = ((i%61)-30)*1000;
      test_result[i] = 0;
    }

  apply_volume(volume, test_input1, test_result, TEST_LENGTH);

  for (i = 0; i < TEST_LENGTH; i++)
    {
	printf("test result %d * %d = %d (sample %d)\n",
	       test_input1[i], volume, test_result[i], i);
    }

  return 0;
}

int main (int argc, char * argv[]) {
    //test_interleave(argc, argv);
    //test_deinterleave(argc, argv);
    //test_dup(argc, argv);
  test_downmix(argc, argv);
  //test_extract(argc, argv);
  //test_mix(argc, argv);
  //test_mix_in_with_volume(argc, argv);
  //test_apply_volume(argc, argv);
}
