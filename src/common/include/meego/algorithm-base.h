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

#ifndef _meego_algorithm_base_h_
#define _meego_algorithm_base_h_

#include <pulsecore/core.h>
#include <pulsecore/module.h>
#include <pulsecore/hook-list.h>
#include <pulsecore/modargs.h>

#include <meego/algorithm-hook.h>
#include <meego/parameter-hook.h>

/* Struct used for both algorithm and parameter setup for
 * algorithm base.
 * identifier       - string identifier for algorithm hook slot operations, functions
 * meego_algorithm_base_set_enabled() and meego_algorithm_base_get_hook_slot() use this.
 * priority         - default priority for algorithm hook slot (can be overridden in module arguments)
 * default_argument - default argument for algorithm hook name (can be overridden, may be NULL)
 * cb               - callback for parameter changes and algorithm hook processing.
 *
 * Callback arguments for parameter changes are explained in parameter-hook.h
 * Callback arguments for algorithm hook processing are explained in algorithm-hook.h
 */
struct meego_algorithm_callback_list {
    const char *identifier;
    const char *default_argument;
    pa_hook_priority_t priority;
    pa_hook_cb_t cb;
};

typedef struct meego_algorithm_base_hook meego_algorithm_base_hook;
typedef struct meego_algorithm_base meego_algorithm_base;
typedef struct meego_algorithm_callback_list meego_algorithm_callback_list;

struct meego_algorithm_base {
    pa_core *core;
    pa_module *module;
    pa_modargs *arguments;

    meego_algorithm_hook_api *algorithm;
    PA_LLIST_HEAD(meego_algorithm_base_hook, algorithm_hooks);
    PA_LLIST_HEAD(meego_algorithm_base_hook, parameter_hooks);

    void *userdata;
};


/* Initialize algorithm base with dynamic module parameters.
 *
 * Each parameter_list entry creates module argument parameter_IDENTIFIER,
 * each algorithm_list entry creates module arguments algorithm_IDENTIFIER and priority_IDENTIFIER,
 * where IDENTIFIER is const char* identifier in meego_algorithm_callback_list.
 * parameter_IDENTIFIER and algorithm_IDENTIFIER are string arguments,
 * priority_IDENTIFIER is integer argument.
 *
 * extra_argument_keys is list of strings applied to valid module arguments. The values
 * can be queried after init from meego_algorithm_base->arguments.
 *
 * Initialized meego_algorithm_base struct is set to pa_module struct userdata.
 *
 * Returns initialized pointer meego_algorithm_base in success, NULL if non-valid arguments
 * were provided to module or if implementor parameters were bad. */
meego_algorithm_base* meego_algorithm_base_init(pa_module *m,
                                                const char *const extra_argument_keys[],
                                                const meego_algorithm_callback_list *parameter_list,
                                                const meego_algorithm_callback_list *algorithm_list,
                                                void *userdata);

/* After successful initialization, connect has to be called. Then all the defined parameter and
 * algorithm hooks are connected. */
void meego_algorithm_base_connect(meego_algorithm_base *b);

/* This function should be called in module uninitialization callback, "pa__done()" */
void meego_algorithm_base_done(meego_algorithm_base *b);

/* Set single algorithm hook slot or all hook slots enabled state. */
void meego_algorithm_base_set_enabled(meego_algorithm_base *b, const char *algorithm_identifier, pa_bool_t enabled);
void meego_algorithm_base_set_all_enabled(meego_algorithm_base *b, pa_bool_t enabled);

/* Get pointer to connected hook slot. If this function returns NULL, it means that hook slot
 * with given name is not connected to any hook. */
meego_algorithm_hook_slot *meego_algorithm_base_get_hook_slot(meego_algorithm_base *b, const char *algorithm_identifier);


#endif
