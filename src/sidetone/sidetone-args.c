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

#include <ctype.h>

#include <pulsecore/modargs.h>
#include <pulsecore/core-util.h>
#include "sidetone.h"
#include "sidetone-args.h"

static const char* const valid_modargs[] = {
    "mixer",
    "control_element",
    "master_sink",
    "mainvolume",
    NULL
};

/* parse the main volume table */
int parse_volume_steps(struct mv_volume_steps *steps, const char *step_string) {
    int len;
    int count = 0;
    int i = 0;
    int shift = 0;
    pa_assert(steps);
    if (!step_string)
        return 0;

    len = strlen(step_string);

    while (i < len && count < MAX_STEPS) {
        char step[16], index[16];
        int value, index_value, j, shift;
        size_t start, value_len;

        /* search for next step:value separator */
        for (; i < len && step_string[i] != ':'; i++);

        /* invalid syntax in step string, bail out */
        if (i == len)
            return -1;

        /* increment i by one to get to the start of value */
        i++;

        /* search for next step:value pair separator to determine value string length */
        start = i;
        for (; i < len && step_string[i] != ','; i++);
        value_len = i - start;

        if (value_len < 1 || value_len > sizeof(step)-1)
            return -1;

        /* copy value string part to step string and convert to integer */
        memcpy(step, &step_string[start], value_len);
        step[value_len] = '\0';

        if (pa_atoi(step, &value)) {
            return -1;
        }
        steps->step[count] = value;

        j = start;

        for (; j > -1  && step_string[j--] != ','; shift++);

        /* copy step string part to index string and convert to integer */
        memcpy(index, &step_string[start - shift], shift);
        index[shift - 1] = '\0';

        if (pa_atoi(index, &index_value)) {
            return -1;
        }
        steps->index[count] = index_value;

        shift = -1;
        count++;
    }

    steps->n_steps = count;
    steps->current_step = 0;

    return count;
}


/* parse sidetone configuration file parameters */
sidetone_args* sidetone_args_new(const char *args) {

    pa_modargs* ma = NULL;
    sidetone_args* st_args = NULL;
    int count = 0 ;

    st_args = pa_xnew0(sidetone_args, 1);
    st_args->steps=pa_xnew0(struct mv_volume_steps, 1);
    ma = pa_modargs_new(args, valid_modargs);
    if(!ma) {
        pa_log_error("Failed to parse module arguments");
        goto fail;
    }

    st_args->modargs = ma;

    if(!(st_args->mixer = pa_modargs_get_value(ma, "mixer", NULL))) {
        pa_log_error("Failed to read mixer name");
        goto fail;
    }


    if(!(st_args->control_element = pa_modargs_get_value(ma, "control_element", NULL))) {
        pa_log_error("Failed to parse control element");
        goto fail;
    }


    if( !(st_args->master_sink = pa_modargs_get_value(ma, "master_sink", NULL))) {
        pa_log_error("Failed to parse master sink name");
        goto fail;
    }


    if(!(st_args->mainvolume = pa_modargs_get_value(ma, "mainvolume", NULL))) {
        pa_log_error("failed to search volume string");
    }

    count = parse_volume_steps(st_args->steps, st_args->mainvolume);
    if (count < 1) {
        pa_log_error("failed to parse call steps; %s", st_args->mainvolume);
        goto fail;
    }

    return st_args;

fail:

    sidetone_args_free(st_args);

    return NULL;
}

void sidetone_args_free(sidetone_args *st_args) {

    if(st_args->steps){
       pa_xfree(st_args->steps);
       st_args->steps = NULL;
     }

    /* All single strings are owned by the modargs object */
    if(st_args->modargs) {
        pa_modargs_free(st_args->modargs);
    }

    pa_xfree(st_args);
}

