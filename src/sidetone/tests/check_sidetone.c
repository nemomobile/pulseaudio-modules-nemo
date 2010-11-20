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

#include "sidetone-args.h"


START_TEST (parse_steps)
{
    const char *STRING = "0:0,1:-5,2:-10,3:-10";
    const int expected_count = 4;

    struct mv_volume_steps steps;
    int reply;

    reply = parse_volume_steps(&steps, STRING);

    fail_unless(reply == expected_count, NULL);
}
END_TEST

START_TEST (parse_steps_fail)
{
    const char *STRING = "0few:0f,ew1f:-f5ew,f2:few-10fe,w3few:-10";
    const int expected_count = -1;

    struct mv_volume_steps steps;
    int reply;

    reply = parse_volume_steps(&steps, STRING);

    fail_unless(reply == expected_count, "Reply %d - Expected reply %d", reply, expected_count);
}
END_TEST


START_TEST (parse_steps_verify)
{
    const char *STRING = "7:-2899,6:-1799,5:-1598,4:-1399,3:-1198";
    const int expected_count = 5;
    const int e_step[5] = { -2899, -1799, -1598, -1399, -1198 };
    const int s_step[5] = { 7, 6, 5, 4, 3 };
    int i;

    struct mv_volume_steps steps;
    int reply;

    reply = parse_volume_steps(&steps, STRING);

    fail_unless(reply == expected_count, NULL);

    for (i = 0; i < steps.n_steps; i++) {
        fail_unless(steps.step[i] == e_step[i], NULL);
        fail_unless(steps.index[i] == s_step[i], NULL);
    }
}
END_TEST


START_TEST (parse_steps_empty)
{
    const char *STRING = "";
    const int expected_reply = 0;

    struct mv_volume_steps steps;
    int reply;

    reply = parse_volume_steps(&steps, STRING);

    fail_unless(reply == expected_reply, NULL);
}
END_TEST

START_TEST (parse_steps_malformed1)
{
    const char *STRING = "0:43,2:";
    const int expected_reply = -1;

    struct mv_volume_steps steps;
    int reply;

    reply = parse_volume_steps(&steps, STRING);

    fail_unless(reply == expected_reply, NULL);
}
END_TEST

START_TEST (parse_steps_malformed2)
{
    const char *STRING = "0:43,2:55,";
    const int expected_reply = -1;

    struct mv_volume_steps steps;
    int reply;

    reply = parse_volume_steps(&steps, STRING);

    fail_unless(reply == expected_reply, NULL);
}
END_TEST



Suite *sidetone_suite() {
    Suite *s = suite_create("Sidetone");

    TCase *tc_core = tcase_create("Sidetone");

    /* add test cases */
    tcase_add_test(tc_core, parse_steps);
    tcase_add_test(tc_core, parse_steps_fail);
    tcase_add_test(tc_core, parse_steps_verify);
    tcase_add_test(tc_core, parse_steps_empty);
    tcase_add_test(tc_core, parse_steps_malformed1);
    tcase_add_test(tc_core, parse_steps_malformed2);

    suite_add_tcase(s, tc_core);

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

