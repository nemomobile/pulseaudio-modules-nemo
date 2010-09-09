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
    "sinks",
    "mainvolume",
    NULL
};

int parse_volume_steps(struct mv_volume_steps *steps, const char *step_string) {
    int len;
    int count = 0;
    int i = 0;

    pa_assert(steps);
    if (!step_string)
        return 0;

    len = strlen(step_string);

    while (i < len && count < MAX_STEPS) {
        char step[16];
        int value;
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
        count++;
    }

    steps->n_steps = count;
    steps->current_step = 0;

    return count;
}

/* Parse and allocate a single name from the string pointed to by arg, and
 * advance *arg by the respective amount. Trim away any preceding or following
 * whitespace. 'delimiters' specifies the characters that can separate individual
 * names. */
static char* parse_name(const char **arg, const char *delimiters) {
    size_t len = 0;
    size_t len_trimmed = 0;
    char *result = NULL;
    const char *end = NULL;

    while(isspace(**arg)) {
        (*arg)++;
    }

    len = strcspn(*arg, delimiters);

    if(len == 0) {
        return NULL;
    }

    end = *arg + len;

    /* Back away from the delimiter if one exists */
    if(strchr(delimiters, *end)) {
        end--;
    }

    /* Go back over any whitespaces */
    while(isspace(*end) && end > *arg) {
        end--;
    }

    end++;

    len_trimmed = end - *arg;

    if(len_trimmed == 0) {
        return NULL;
    }

    result = pa_xmalloc0((len_trimmed + 1) * sizeof(char*));
    strncpy(result, *arg, len_trimmed);
    result[len_trimmed] = '\0';

    /* Proceed past the name and the delimiter if we encountered one. */
    if((*arg)[len] == '\0') {
        *arg += len;
    } else {
        *arg += len + 1;
    }

    pa_assert(**arg == '\0' || strchr(delimiters, (*(*arg - 1))));

    return result;
}

/* Parse sink or source names.
 *
 * \param[out] names The resulting array of names. Allocated by this function.
 * \arg List of sinks or sources given as <name>{","<name>} */
static int parse_names(const char ***names, const char *arg) {
    pa_assert(!*names);

    char* name = NULL;
    const char* state = NULL;
    int num_names = 0;
    int array_size = 4;

    if(!arg) {
        return -1;
    }

    *names = (const char**)pa_xmalloc0(array_size * sizeof(char*));

    while((name = parse_name(&arg, ","))) {

        num_names++;

        if(num_names > array_size) {
            array_size *= 2;
            *names = (const char**)pa_xrealloc(*names, array_size * sizeof(char*));
        }

        (*names)[num_names - 1] = name;
    }

    return num_names;
}

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


    if((st_args->num_sinks = parse_names(&st_args->sinks, pa_modargs_get_value(ma, "sinks", NULL))) == 0) {
        pa_log_error("Failed to parse sink names");
        goto fail;
    }


    if(!(st_args->mainvolume = pa_modargs_get_value(ma, "mainvolume", NULL))) {
        pa_log_error("failed to search volume string");
    }

    count = parse_volume_steps(st_args->steps,st_args->mainvolume);
    if (count < 1) {
        pa_log_warn("failed to parse call steps; %s", st_args->mainvolume);
    }

    return st_args;

fail:

    sidetone_args_free(st_args);

    return NULL;
}

void sidetone_args_free(sidetone_args *st_args) {

    int i = 0;

    if(st_args->sinks) {
        for(i = 0; i < st_args->num_sinks; i++) {
            pa_xfree((char*)st_args->sinks[i]);
        }
        pa_xfree(st_args->sinks);
    }

    if(st_args->steps){
       pa_xfree(st_args->steps);
       st_args->steps=NULL;
     }

    /* All single strings are owned by the modargs object */
    if(st_args->modargs) {
        pa_modargs_free(st_args->modargs);
    }

    pa_xfree(st_args);
}

