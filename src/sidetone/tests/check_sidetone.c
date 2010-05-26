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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <check.h>

#include <pulsecore/core-util.h>

#include "sidetone-args.h"

/* Verify an array against a reference array using cmp_function for comparison.
 * Implemented as a macro in order to bypass type constraints. */
#define TEST_ARRAY(array, reference, len, cmp_function)                                     \
    do {                                                                                    \
        int i;                                                                              \
        for(i = 0; i < len; i++) {                                                          \
            cmp_function(array[i], reference[i]);                                           \
        }                                                                                   \
    } while(0)

/* Comparison functions for the above macro */

static void channel_equals(snd_mixer_selem_channel_id_t c1, snd_mixer_selem_channel_id_t c2) {
    fail_unless(c1 == c2, "Channels %d and %d do not match", c1, c2);
}

static void string_equals(const char* s1, const char* s2) {
    fail_unless(pa_streq(s1, s2), "Strings \"%s\" and \"%s\" do not match", s1, s2);
}

START_TEST(valid_args)
{
    sidetone_args *args = NULL;
    args = sidetone_args_new("mixer=\"Mega Mixer\" "
                             "input_elements=in1,in2,in3 "
                             "output_elements=\"out elem 1:1,out elem 2:0\" "
                             "control_element=\"We are now controlling the transmission\" "
                             "target_volume=400 "
                             "sinks=sink.test "
                             "sources=source.larry,source.moe,source.curly");

    fail_unless(args != NULL);
    fail_unless(pa_streq(args->mixer, "Mega Mixer"));
    fail_unless(pa_streq(args->control_element, "We are now controlling the transmission"));

    /* Verify input elements */
    {
        const char* ref[] = {"in1", "in2", "in3"};
        snd_mixer_selem_channel_id_t channel_ref[] = { SND_MIXER_SCHN_MONO, SND_MIXER_SCHN_MONO, SND_MIXER_SCHN_MONO};

        fail_unless(args->num_input_elements == 3);

        TEST_ARRAY(args->input_elements, ref, args->num_input_elements, string_equals);
        TEST_ARRAY(args->input_channels, channel_ref, args->num_input_elements, channel_equals);
    }

    /* Verify output elements */
    {
        const char* ref[] = {"out elem 1", "out elem 2"};
        snd_mixer_selem_channel_id_t channel_ref[] = { SND_MIXER_SCHN_FRONT_RIGHT, SND_MIXER_SCHN_MONO };

        fail_unless(args->num_output_elements == 2);

        TEST_ARRAY(args->output_elements, ref, args->num_output_elements, string_equals);
        TEST_ARRAY(args->output_channels, channel_ref, args->num_output_elements, channel_equals);
    }

    fail_unless(args->target_volume == 400);

    /* Verify sinks and sources */
    {
        const char* ref_sink[] = {"sink.test"};
        const char* ref_source[] = {"source.larry", "source.moe", "source.curly"};

        fail_unless(args->num_sinks == 1);
        fail_unless(args->num_sources == 3);

        TEST_ARRAY(args->sinks, ref_sink, args->num_sinks, string_equals);
        TEST_ARRAY(args->sources, ref_source, args->num_sources, string_equals);
    }

    sidetone_args_free(args);
}
END_TEST

START_TEST(valid_args_long_lists)
{
    sidetone_args *args = NULL;
    args = sidetone_args_new("mixer=\"Mega Mixer\" "
                             "input_elements=\"Front left:0,Front right:1,Rear left:2,Rear right:3,Front center:4,Woofer:5,Side Left:6,Side Right:7,Rear Center:8,"
                                              "Front left:0,Front right:1,Rear left:2,Rear right:3,Front center:4,Woofer:5,Side Left:6,Side Right:7,Rear Center:8,"
                                              "Front left:0,Front right:1,Rear left:2,Rear right:3,Front center:4,Woofer:5,Side Left:6,Side Right:7,Rear Center:8,"
                                              "Front left:0,Front right:1,Rear left:2,Rear right:3,Front center:4,Woofer:5,Side Left:6,Side Right:7,Rear Center:8\" "
                             "output_elements=\"out elem 1:1,out elem 2:0\" "
                             "control_element=\"We are now controlling the transmission\" "
                             "target_volume=0  "
                             "sinks=sink.test "
                             "sources=source.larry,source.moe,source.curly");

    fail_unless(args != NULL);

    /* Verify input elements*/
    {
        const char** pos = args->input_elements;
        snd_mixer_selem_channel_id_t* pos_channels = args->input_channels;

        const char* ref[] = {"Front left", "Front right", "Rear left", "Rear right", 
                             "Front center", "Woofer", "Side Left", "Side Right", "Rear Center"};

        snd_mixer_selem_channel_id_t ref_channels[] = {
            SND_MIXER_SCHN_FRONT_LEFT, SND_MIXER_SCHN_FRONT_RIGHT,
            SND_MIXER_SCHN_REAR_LEFT, SND_MIXER_SCHN_REAR_RIGHT,
            SND_MIXER_SCHN_FRONT_CENTER, SND_MIXER_SCHN_WOOFER,
            SND_MIXER_SCHN_SIDE_LEFT, SND_MIXER_SCHN_SIDE_RIGHT,
            SND_MIXER_SCHN_REAR_CENTER
        };

        fail_unless(args->num_input_elements == 9 * 4 );

        while(pos < args->input_elements + 9 * 4) {
            TEST_ARRAY(pos, ref, 9, string_equals);
            TEST_ARRAY(pos_channels, ref_channels, 9, channel_equals);
            pos += 9;
            pos_channels += 9;
        }
    }

    sidetone_args_free(args);
}
END_TEST

START_TEST(valid_args_names_surrounded_by_whitespace)
{
    sidetone_args *args = NULL;

    args = sidetone_args_new("mixer=\"Mega Mixer\" "
                             "input_elements=\"    in1   ,  in2,     in3 \" "
                             "output_elements=\"out elem 1 :    1  ,         out elem 2  :       0\" "
                             "control_element=\"We are now controlling the transmission\" "
                             "target_volume=0  "
                             "sinks=\"                  sink.test \" "
                             "sources=\"source.larry  ,source.moe,          source.curly  \"");

    fail_unless(args != NULL);

    /* Verify input elements */
    {
        const char* ref[] = {"in1", "in2", "in3"};
        snd_mixer_selem_channel_id_t channel_ref[] = { SND_MIXER_SCHN_MONO, SND_MIXER_SCHN_MONO, SND_MIXER_SCHN_MONO};

        fail_unless(args->num_input_elements == 3);

        TEST_ARRAY(args->input_elements, ref, args->num_input_elements, string_equals);
        TEST_ARRAY(args->input_channels, channel_ref, args->num_input_elements, channel_equals);
    }

    /* Verify output elements */
    {
        const char* ref[] = {"out elem 1", "out elem 2"};
        snd_mixer_selem_channel_id_t channel_ref[] = { SND_MIXER_SCHN_FRONT_RIGHT, SND_MIXER_SCHN_MONO };

        fail_unless(args->num_output_elements == 2);

        TEST_ARRAY(args->output_elements, ref, args->num_output_elements, string_equals);
        TEST_ARRAY(args->output_channels, channel_ref, args->num_output_elements, channel_equals);
    }

    fail_unless(args->target_volume == 0);

    /* Verify sinks and sources */
    {
        const char* ref_sink[] = {"sink.test"};
        const char* ref_source[] = {"source.larry", "source.moe", "source.curly"};

        fail_unless(args->num_sinks == 1);
        fail_unless(args->num_sources == 3);

        TEST_ARRAY(args->sinks, ref_sink, args->num_sinks, string_equals);
        TEST_ARRAY(args->sources, ref_source, args->num_sources, string_equals);
    }

    sidetone_args_free(args);
}
END_TEST

START_TEST(invalid_args_channel_delimiter_without_a_channel)
{
    sidetone_args *args = NULL;

    args = sidetone_args_new("mixer=\"Mega Mixer\" "
                             "input_elements=in1,in2:,in3 "
                             "output_elements=\"out elem 1:1,out elem 2:0\" "
                             "control_element=\"We are now controlling the transmission\" "
                             "target_volume=0  "
                             "sinks=sink.test "
                             "sources=source.larry,source.moe,source.curly");

    fail_if(args != NULL);
}
END_TEST

START_TEST(invalid_args_two_consecutive_delimiters)
{
    sidetone_args *args = NULL;

    args = sidetone_args_new("mixer=\"Mega Mixer\" "
                             "input_elements=in1,in2,in3 "
                             "output_elements=\"out elem 1:1,,out elem 2:0\" "
                             "control_element=\"We are now controlling the transmission\" "
                             "target_volume=0  "
                             "sinks=sink.test "
                             "sources=source.larry,source.moe,source.curly");

    fail_if(args != NULL);
}
END_TEST

START_TEST(invalid_args_target_volume_not_a_number)
{ 
    sidetone_args *args = NULL;

    args = sidetone_args_new("mixer=\"Mega Mixer\" "
                             "input_elements=in1,in2,in3 "
                             "output_elements=\"out elem 1:1,out elem 2:0\" "
                             "control_element=\"We are now controlling the transmission\" "
                             "target_volume=a0 "
                             "sinks=sink.test "
                             "sources=source.larry,source.moe,source.curly");
    fail_if(args != NULL);
}
END_TEST

Suite *sidetone_suite(void) {
    Suite *s = suite_create("Sidetone");
    TCase *tc = tcase_create("Sidetone");

    /* add test cases */
    tcase_add_test(tc, valid_args);
    tcase_add_test(tc, valid_args_long_lists);
    tcase_add_test(tc, valid_args_names_surrounded_by_whitespace);
    tcase_add_test(tc, invalid_args_channel_delimiter_without_a_channel);
    tcase_add_test(tc, invalid_args_two_consecutive_delimiters);
    tcase_add_test(tc, invalid_args_target_volume_not_a_number);

    suite_add_tcase(s, tc);
    return s;
}

int main(void) {
    int number_failed;
    Suite *s = sidetone_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

