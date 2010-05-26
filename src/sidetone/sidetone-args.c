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

#include "sidetone-args.h"

static const char* const valid_modargs[] = {
    "mixer",
    "input_elements",
    "output_elements",
    "control_element",
    "target_volume",
    "sinks",
    "sources",
    NULL
};

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

/* Parse a single channel from the string pointed to by arg, and advance *arg
 * by the respective amount */
static int parse_channel(const char **arg) {
    size_t len = 0;
    size_t num_end = 0;
    int result = 0;
    char tmp[32];

    len = strcspn(*arg, ",:");

    if(len == 0 || len > 31 || (*arg)[len] == ':') {
        return -1;
    }

    num_end = len;

    /* Back away from the delimiter */
    if((*arg)[num_end] == ',') {
        num_end--;
    }

    /* pa_atoi handles preceding whitespaces but not the trailing ones, so
     * we need to go back over those */
    while(isspace((*arg)[num_end])) {
        num_end--;
    }

    num_end++;

    strncpy(tmp, *arg, num_end);
    tmp[num_end] = '\0';

    if(pa_atoi(tmp, &result) < 0) {
        return -1;
    }

    if((*arg)[len] == '\0') {
        *arg += len;
    } else {
        *arg += len + 1;
    }

    pa_assert(**arg == '\0' || *(*arg - 1) == ',');

    return result;
}

/* Parse mixer elements and channels.
 *
 * \param[out] elements Parsed element names. Allocated by this function.
 * \param[out] channels Parsed channel numbers. Allocated by this function.
 * \param arg Element specification given as <element_name>[":"<channel>]{","<element_name>[":"<channel>]} */
static int parse_elements(const char ***elements, snd_mixer_selem_channel_id_t **channels, const char *arg) {
    pa_assert(!*elements);
    pa_assert(!*channels);

    int channel = 0;
    int num_elements = 0;
    int array_size = 10;
    const char *element = NULL;

    if(!arg) {
        pa_log_error("No mixer elements specified");
        return -1;
    }

    *elements = (const char**)pa_xmalloc0(array_size * sizeof(char*));
    *channels = (snd_mixer_selem_channel_id_t*)pa_xmalloc0(array_size * sizeof(snd_mixer_selem_channel_id_t*));

    while((element = parse_name(&arg, ",:")) != NULL) {

        num_elements++;

        if(num_elements > array_size) {
            array_size *= 2;
            *elements = (const char**)pa_xrealloc(*elements, array_size * sizeof(char*));
            *channels = (snd_mixer_selem_channel_id_t*)pa_xrealloc(*channels, array_size * sizeof(snd_mixer_selem_channel_id_t*));
        }

        (*elements)[num_elements - 1] = element;

        /* If a channel is specified, parse it */
        if(*(arg - 1) == ':') {

            if((channel = parse_channel(&arg)) < 0 || channel > SND_MIXER_SCHN_LAST) {
                pa_log_error("Malformed channel number at \"%s\"", arg);
                goto fail;
            }

            (*channels)[num_elements - 1] = (snd_mixer_selem_channel_id_t)channel;
        } else {
            /* Use mono by default */
            (*channels)[num_elements - 1] = SND_MIXER_SCHN_MONO;
        }
    }

    if(*arg) {
        pa_log_error("Failed to parse elements at: \"%s\"", arg);
        goto fail;
    }

    return num_elements;

fail:

    if(*elements) {
        int i;
        for(i = 0; i < num_elements; i++) {
            pa_xfree((char*)(*elements)[i]);
        }
        pa_xfree(*elements);
        *elements = NULL;
    }

    if(*channels) {
        pa_xfree(*channels);
        *channels = NULL;
    }

    return -1;
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
    const char* num_str = NULL;
    char* num_end = NULL;

    st_args = pa_xnew0(sidetone_args, 1);
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

    if((st_args->num_input_elements = parse_elements(&st_args->input_elements, &st_args->input_channels,
                                                    pa_modargs_get_value(ma, "input_elements", NULL))) <= 0) {
        pa_log_error("Failed to parse input elements");
        goto fail;
    }

    if((st_args->num_output_elements = parse_elements(&st_args->output_elements, &st_args->output_channels,
                                                     pa_modargs_get_value(ma, "output_elements", NULL))) <= 0) {
        pa_log_error("Failed to parse output elements");
        goto fail;
    }

    if(!(st_args->control_element = pa_modargs_get_value(ma, "control_element", NULL))) {
        pa_log_error("Failed to parse control element");
        goto fail;
    }

    if(!(num_str = pa_modargs_get_value(ma, "target_volume", NULL))) {
        pa_log_error("Failed to parse target volume");
        goto fail;
    }

    st_args->target_volume = strtol(num_str, &num_end, 10);
    if(st_args->target_volume == 0 && num_end - num_str < strlen(num_str)) {
        pa_log_error("Malformed volume value: %s", num_str);
        goto fail;
    }

    if((st_args->num_sinks = parse_names(&st_args->sinks, pa_modargs_get_value(ma, "sinks", NULL))) == 0) {
        pa_log_error("Failed to parse sink names");
        goto fail;
    }

    /* Sources aren't mandatory.  */
    if((st_args->num_sources = parse_names(&st_args->sources, pa_modargs_get_value(ma, "sources", NULL))) == 0) {
        pa_log_debug("No sources specified");
    }

    return st_args;

fail:

    sidetone_args_free(st_args);

    return NULL;
}

void sidetone_args_free(sidetone_args *st_args) {

    int i = 0;

    if(st_args->input_elements) {
        for(i = 0; i < st_args->num_input_elements; i++) {
            pa_xfree((char*)st_args->input_elements[i]);
        }
        pa_xfree(st_args->input_elements);
        pa_xfree(st_args->input_channels);
    }

    if(st_args->output_elements) {
        for(i = 0; i < st_args->num_output_elements; i++) {
            pa_xfree((char*)st_args->output_elements[i]);
        }
        pa_xfree(st_args->output_elements);
        pa_xfree(st_args->output_channels);
    }

    if(st_args->sinks) {
        for(i = 0; i < st_args->num_sinks; i++) {
            pa_xfree((char*)st_args->sinks[i]);
        }
        pa_xfree(st_args->sinks);
    }

    if(st_args->sources) {
        for(i = 0; i < st_args->num_sources; i++) {
            pa_xfree((char*)st_args->sources[i]);
        }
        pa_xfree(st_args->sources);
    }

    /* All single strings are owned by the modargs object */
    if(st_args->modargs) {
        pa_modargs_free(st_args->modargs);
    }

    pa_xfree(st_args);
}

