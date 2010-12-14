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
#include <stdio.h>
#include <check.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/hashmap.h>
#include <pulse/volume.h>

#include "mainvolume.h"

#define COUNT (10)

static void init_steps(int *steps, int count) {
    int i;

    for (i = 0; i < count; i++) {
        steps[i] = i * 100;
    }
}

START_TEST (search_step_exact)
{
    const int expected_step = 2;
    int steps[COUNT];
    int step;

    init_steps(steps, COUNT);

    step = mv_search_step(steps, COUNT, 200);

    fail_unless(step == expected_step, "Expected step %d - found step %d", expected_step, step);
}
END_TEST

START_TEST (search_step_near_negative)
{
    const int expected_step = 2;
    int steps[COUNT];
    int step;

    init_steps(steps, COUNT);

    step = mv_search_step(steps, COUNT, 190);

    fail_unless(step == expected_step, "Expected step %d - found step %d", expected_step, step);
}
END_TEST

START_TEST (search_step_near_positive)
{
    const int expected_step = 3;
    int steps[COUNT];
    int step;

    init_steps(steps, COUNT);

    step = mv_search_step(steps, COUNT, 210);

    fail_unless(step == expected_step, "Expected step %d - found step %d", expected_step, step);
}
END_TEST

START_TEST (search_step_between)
{
    const int expected_step = 3;
    int steps[COUNT];
    int step;

    init_steps(steps, COUNT);

    step = mv_search_step(steps, COUNT, 250);

    fail_unless(step == expected_step, "Expected step %d - found step %d", expected_step, step);
}
END_TEST

START_TEST (search_step_max)
{
    const int expected_step = 9;
    int steps[COUNT];
    int step;

    init_steps(steps, COUNT);

    step = mv_search_step(steps, COUNT, 900);

    fail_unless(step == expected_step, "Expected step %d - found step %d", expected_step, step);
}
END_TEST

START_TEST (search_step_out_of_bounds)
{
    const int expected_step = COUNT-1;
    int steps[COUNT];
    int step;

    init_steps(steps, COUNT);

    step = mv_search_step(steps, COUNT, 80000);

    fail_unless(step == expected_step, "Expected to fail, step %d - found step %d", expected_step, step);

}
END_TEST

START_TEST (search_step_out_of_bounds_negative)
{
    const int expected_step = 0;
    int steps[COUNT];
    int step;

    init_steps(steps, COUNT);

    step = mv_search_step(steps, COUNT, -5);

    fail_unless(step == expected_step, "Expected to fail, step %d - found step %d", expected_step, step);

}
END_TEST

START_TEST (search_step_range)
{
    const int unexpected_step = -1;
    const int n_steps = 2;
    int steps[n_steps];
    int step;
    int i;

    steps[0] = 400;
    steps[1] = 40000;

    for (i = -600; i < 600000; i++) {

        step = mv_search_step(steps, n_steps, i);

        /* had to check this myself first, if fail_unless is in loop
         * make check results in Killed
         * strange.
         */
        if (step == unexpected_step) {
            fail_unless(step > unexpected_step, "Expected not to fail, value %d , step %d - expected step to be valid", i, step);
            break;
        }
    }
}
END_TEST

START_TEST (parse_steps)
{
    const char *STRING = "0:0,1:-5,2:-10,3:-10";
    const int expected_count = 4;

    struct mv_volume_steps steps;
    int reply;

    reply = mv_parse_single_steps(&steps, STRING);

    fail_unless(reply == expected_count, NULL);
}
END_TEST

START_TEST (parse_steps_fail)
{
    const char *STRING = "0few:0f,ew1f:-f5ew,f2:few-10fe,w3few:-10";
    const int expected_count = -1;

    struct mv_volume_steps steps;
    int reply;

    reply = mv_parse_single_steps(&steps, STRING);

    fail_unless(reply == expected_count, "Reply %d - Expected reply %d", reply, expected_count);
}
END_TEST

START_TEST (parse_steps_verify)
{
    const char *STRING = "0:-1500,1:-1000,2:-500,3:0";
    const int expected_count = 4;
    const int e_step[4] = { -1500, -1000, -500, 0};
    int i;

    struct mv_volume_steps steps;
    int reply;

    reply = mv_parse_single_steps(&steps, STRING);

    fail_unless(reply == expected_count, NULL);

    for (i = 0; i < steps.n_steps; i++) {
        fail_unless(steps.step[i] == e_step[i], NULL);
    }
}
END_TEST

START_TEST (normalize)
{
    int i;
    struct mv_volume_steps steps;
    const int e_step[3] = {
        0,
        pa_sw_volume_from_dB(-3000.0 / 100.0),
        pa_sw_volume_from_dB(0.0)
    };
    steps.n_steps = 3;
    steps.step[0] = -20000; /* mute treshold is -20000 mB */
    steps.step[1] = -3000;
    steps.step[2] = 0;

    mv_normalize_steps(&steps);

    for (i = 0; i < steps.n_steps; i++)
        fail_unless(steps.step[i] == e_step[i], "Step[%d] value %d - expected value %d", i, steps.step[i], e_step[i]);
}
END_TEST

START_TEST (parse_steps_verify_normalize)
{
    const char *STRING = "0:-1500,1:0";
    const int expected_count = 2;
    const int e_step[2] = {
        pa_sw_volume_from_dB(-1500.0 / 100.0),
        pa_sw_volume_from_dB(0.0)
    };

    struct mv_volume_steps steps;
    int reply;

    reply = mv_parse_single_steps(&steps, STRING);
    mv_normalize_steps(&steps);

    fail_unless(reply == expected_count, NULL);
    fail_unless(steps.step[0] == e_step[0], NULL);
    fail_unless(steps.step[1] == e_step[1], NULL);
}
END_TEST

START_TEST (parse_steps_empty)
{
    const char *STRING = "";
    const int expected_reply = 0;

    struct mv_volume_steps steps;
    int reply;

    reply = mv_parse_single_steps(&steps, STRING);

    fail_unless(reply == expected_reply, NULL);
}
END_TEST

START_TEST (parse_steps_malformed1)
{
    const char *STRING = "0:43,2:";
    const int expected_reply = -1;

    struct mv_volume_steps steps;
    int reply;

    reply = mv_parse_single_steps(&steps, STRING);

    fail_unless(reply == expected_reply, NULL);
}
END_TEST

START_TEST (parse_steps_malformed2)
{
    const char *STRING = "0:43,2:55,";
    const int expected_reply = -1;

    struct mv_volume_steps steps;
    int reply;

    reply = mv_parse_single_steps(&steps, STRING);

    fail_unless(reply == expected_reply, NULL);
}
END_TEST

Suite *mainvolume_suite() {
    Suite *s = suite_create("MainVolume");

    TCase *tc_core = tcase_create("Core");

    /* add test cases */
    tcase_add_test(tc_core, search_step_exact);
    tcase_add_test(tc_core, search_step_near_negative);
    tcase_add_test(tc_core, search_step_near_positive);
    tcase_add_test(tc_core, search_step_between);
    tcase_add_test(tc_core, search_step_max);
    tcase_add_test(tc_core, search_step_out_of_bounds);
    tcase_add_test(tc_core, search_step_out_of_bounds_negative);
    tcase_add_test(tc_core, search_step_range);
    tcase_add_test(tc_core, parse_steps);
    tcase_add_test(tc_core, parse_steps_fail);
    tcase_add_test(tc_core, parse_steps_verify);
    tcase_add_test(tc_core, parse_steps_empty);
    tcase_add_test(tc_core, parse_steps_malformed1);
    tcase_add_test(tc_core, parse_steps_malformed2);
    tcase_add_test(tc_core, normalize);
    tcase_add_test(tc_core, parse_steps_verify_normalize);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void) {
    int number_failed;
    Suite *s = mainvolume_suite();
    SRunner *sr = srunner_create(s);
    srunner_run_all(sr, CK_VERBOSE);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

