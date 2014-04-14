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

#include <pulse/xmalloc.h>
#include <pulse/proplist.h>
#include <pulsecore/core-util.h>
#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/hook-list.h>

#include "algorithm-hook.h"
#include "parameter-hook.h"

#include "algorithm-base.h"

/* Dynamic base strings for parameter and algorithm module arguments. */
#define PARAMETER_HOOK_NAME_STR_PARAMETER   "parameter_%s"
#define PARAMETER_HOOK_NAME_STR_ALGORITHM   "algorithm_%s"
#define PARAMETER_HOOK_PRIORITY_STR         "priority_%s"

struct meego_algorithm_base_hook {
    /* Used for initialization, these
     * strings will be freed after
     * successfull initialization. */
    char *default_hook_name;
    char *parameter_hook_name;
    char *parameter_priority;


    bool enabled;
    char *identifier;
    char *hook_name;

    pa_hook_priority_t priority;
    pa_hook_cb_t cb;
    meego_algorithm_hook_slot *algorithm_hook_slot;

    PA_LLIST_FIELDS(meego_algorithm_base_hook);
};

static void clean_init_data(meego_algorithm_base_hook *list) {
    meego_algorithm_base_hook *c = NULL;

    PA_LLIST_FOREACH(c, list) {
        if (c->default_hook_name) {
            pa_xfree(c->default_hook_name);
            c->default_hook_name = NULL;
        }

        if (c->parameter_hook_name) {
            pa_xfree(c->parameter_hook_name);
            c->parameter_hook_name = NULL;
        }

        if (c->parameter_priority) {
            pa_xfree(c->parameter_priority);
            c->parameter_priority = NULL;
        }
    }
}

static void free_hooks(meego_algorithm_base_hook *list) {
    meego_algorithm_base_hook *c = NULL;

    clean_init_data(list);

    while ((c = list)) {
        PA_LLIST_REMOVE(meego_algorithm_base_hook, list, c);
        if (c->algorithm_hook_slot)
            meego_algorithm_hook_slot_free(c->algorithm_hook_slot);
        if (c->identifier)
            pa_xfree(c->identifier);
        if (c->hook_name)
            pa_xfree(c->hook_name);
        pa_xfree(c);
        c = NULL;
    }
}

static int parse_hook_names_dynamic(meego_algorithm_base *b, pa_module *m,
                                    const char *const extra_argument_keys[],
                                    const meego_algorithm_callback_list *parameter_list,
                                    const meego_algorithm_callback_list *algorithm_list) {
    int argument_count = 0;
    int arg = 0;
    pa_modargs *ma = NULL;
    const char ** valid_modargs = NULL;
    const meego_algorithm_callback_list *i;
    meego_algorithm_base_hook *c;

    pa_assert(b);
    pa_assert(m);

    if (parameter_list) {
        for (i = parameter_list; i && i->identifier; i++) {
            c = pa_xnew0(meego_algorithm_base_hook, 1);
            PA_LLIST_INIT(meego_algorithm_base_hook, c);

            c->enabled = true;
            c->identifier = pa_xstrdup(i->identifier);
            c->parameter_hook_name = pa_sprintf_malloc(PARAMETER_HOOK_NAME_STR_PARAMETER, i->identifier);
            if (i->default_argument)
                c->default_hook_name = pa_xstrdup(i->default_argument);
            c->priority = i->priority;
            c->cb = i->cb;

            PA_LLIST_PREPEND(meego_algorithm_base_hook, b->parameter_hooks, c);
            pa_log_debug("Adding argument %s", c->parameter_hook_name);
            argument_count++;
        }
    }

    if (algorithm_list) {
        for (i = algorithm_list; i && i->identifier; i++) {
            c = pa_xnew0(meego_algorithm_base_hook, 1);
            PA_LLIST_INIT(meego_algorithm_base_hook, c);

            c->enabled = true;
            c->identifier = pa_xstrdup(i->identifier);
            c->parameter_hook_name = pa_sprintf_malloc(PARAMETER_HOOK_NAME_STR_ALGORITHM, i->identifier);
            c->parameter_priority = pa_sprintf_malloc(PARAMETER_HOOK_PRIORITY_STR, i->identifier);
            if (i->default_argument)
                c->default_hook_name = pa_xstrdup(i->default_argument);
            c->priority = i->priority;
            c->cb = i->cb;

            PA_LLIST_PREPEND(meego_algorithm_base_hook, b->algorithm_hooks, c);
            pa_log_debug("Adding argument %s (default priority %d)", c->parameter_hook_name, c->priority);

            /* one algorithm hook name and one priority parameter */
            argument_count += 2;
        }
    }

    /* Count additional arguments */
    if (extra_argument_keys) {
        unsigned k;
        for (k = 0; extra_argument_keys[k]; k++)
            argument_count++;

        pa_log_debug("Adding %u module defined arguments.", k);
    }

    if (argument_count == 0) {
        pa_log_error("No parameter or algorithm hooks or extra module arguments defined in implementor.");
        goto fail;
    }

    valid_modargs = pa_xnew0(const char*, argument_count + 1);
    valid_modargs[argument_count] = NULL;

    PA_LLIST_FOREACH(c, b->parameter_hooks)
        valid_modargs[arg++] = c->parameter_hook_name;

    PA_LLIST_FOREACH(c, b->algorithm_hooks) {
        valid_modargs[arg++] = c->parameter_hook_name;
        valid_modargs[arg++] = c->parameter_priority;
    }

    if (extra_argument_keys) {
        unsigned k;
        for (k = 0; extra_argument_keys[k]; k++)
            valid_modargs[arg++] = extra_argument_keys[k];
    }

    pa_assert(arg == argument_count);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    PA_LLIST_FOREACH(c, b->parameter_hooks) {
        const char *v;
        if (!(v = pa_modargs_get_value(ma, c->parameter_hook_name, NULL))) {
            if (c->default_hook_name)
                v = c->default_hook_name;
            else {
                pa_log_info("Missing argument for parameter hook %s", c->parameter_hook_name);
                c->enabled = false;
            }
        }
        c->hook_name = pa_xstrdup(v);
    }

    PA_LLIST_FOREACH(c, b->algorithm_hooks) {
        const char *v;
        int32_t priority;

        if (!(v = pa_modargs_get_value(ma, c->parameter_hook_name, NULL))) {
            if (c->default_hook_name)
                v = c->default_hook_name;
            else {
                pa_log_info("Missing argument for algorithm hook %s", c->parameter_hook_name);
                c->enabled = false;
            }
        }
        c->hook_name = pa_xstrdup(v);

        if ((v = pa_modargs_get_value(ma, c->parameter_priority, NULL))) {
            if (pa_atoi(v, &priority) < 0) {
                pa_log_error("Failed to get value for priority %s", c->parameter_priority);
                goto fail;
            } else {
                pa_log_debug("Updating algorithm hook %s priority %d", c->hook_name, priority);
                c->priority = priority;
            }
        }
    }

    pa_xfree(valid_modargs);

    /* Save module arguments if algorithm module implementor
     * needs it's extra arguments. */
    b->arguments = ma;

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    if (valid_modargs)
        pa_xfree(valid_modargs);

    return -1;
}

static meego_algorithm_base* new_base(pa_module *m, void *userdata) {
    meego_algorithm_base *b;

    pa_assert(m);

    b = pa_xnew0(meego_algorithm_base, 1);

    m->userdata = b;
    b->core = m->core;
    b->module = m;
    b->userdata = userdata;

    PA_LLIST_HEAD_INIT(meego_algorithm_base_hook, b->algorithm_hooks);
    PA_LLIST_HEAD_INIT(meego_algorithm_base_hook, b->parameter_hooks);

    return b;
}

meego_algorithm_base* meego_algorithm_base_init(pa_module *m,
                                                const char *const extra_argument_keys[],
                                                const meego_algorithm_callback_list *parameter_list,
                                                const meego_algorithm_callback_list *algorithm_list,
                                                void *userdata) {
    meego_algorithm_base *b;

    pa_assert(m);

    b = new_base(m, userdata);

    if (parse_hook_names_dynamic(b, m, extra_argument_keys, parameter_list, algorithm_list) < 0) {
        pa_log_error("Failed to parse dynamic hook names.");
        goto fail;
    }

    if (b->algorithm_hooks) {
        b->algorithm = meego_algorithm_hook_api_get(b->core);
        if (!b->algorithm) {
            pa_log("Failed to get algorithm interface.");
            goto fail;
        }
    }

    return b;

 fail:
    free_hooks(b->algorithm_hooks);
    free_hooks(b->parameter_hooks);

    if (b->algorithm) {
        meego_algorithm_hook_api_unref(b->algorithm);
        b->algorithm = NULL;
    }

    if (b)
        pa_xfree(b);

    m->userdata = NULL;

    return NULL;
}

void meego_algorithm_base_connect(meego_algorithm_base *b) {
    unsigned count = 0;
    meego_algorithm_base_hook *i;

    pa_assert(b);

    pa_log_info("(Connected) algorithm hooks:");

    PA_LLIST_FOREACH(i, b->algorithm_hooks) {
        if (!i->enabled)
            continue;

        i->algorithm_hook_slot = meego_algorithm_hook_connect(b->algorithm, i->hook_name, i->priority, i->cb, b->userdata);
        pa_log_info("[%s] %s: %s (priority %d)", i->algorithm_hook_slot ? "X" : " ", i->parameter_hook_name, i->hook_name, i->priority);
        count++;
    }

    PA_LLIST_FOREACH(i, b->parameter_hooks) {
        if (!i->enabled)
            continue;

        pa_log_info("Request parameter updates for %s", i->hook_name);
        meego_parameter_request_updates(i->hook_name, i->cb, i->priority, false, b->userdata);
        count++;
    }

    if (count == 0)
        pa_log_warn("No connected algorithm or parameter hooks! (This module will do nothing)");

    clean_init_data(b->algorithm_hooks);
    clean_init_data(b->parameter_hooks);
}

void meego_algorithm_base_done(meego_algorithm_base *b) {
    meego_algorithm_base_hook *i;

    pa_assert(b);

    /* Stop parameter updates */
    PA_LLIST_FOREACH(i, b->parameter_hooks)
        meego_parameter_stop_updates(i->hook_name, i->cb, b->userdata);

    free_hooks(b->algorithm_hooks);
    free_hooks(b->parameter_hooks);

    if (b->algorithm)
        meego_algorithm_hook_api_unref(b->algorithm);

    if (b->arguments)
        pa_modargs_free(b->arguments);

    pa_xfree(b);
}

void meego_algorithm_base_set_enabled(meego_algorithm_base *b, const char *algorithm_identifier, bool enabled) {
    meego_algorithm_base_hook *i;

    pa_assert(b);
    pa_assert(algorithm_identifier);

    PA_LLIST_FOREACH(i, b->algorithm_hooks) {
        if (pa_streq(i->identifier, algorithm_identifier) && i->algorithm_hook_slot) {
            meego_algorithm_hook_slot_set_enabled(i->algorithm_hook_slot, enabled);
            break;
        }
    }
}

void meego_algorithm_base_set_all_enabled(meego_algorithm_base *b, bool enabled) {
    meego_algorithm_base_hook *i;

    pa_assert(b);

    PA_LLIST_FOREACH(i, b->algorithm_hooks) {
        if (i->algorithm_hook_slot)
            meego_algorithm_hook_slot_set_enabled(i->algorithm_hook_slot, enabled);
    }
}

meego_algorithm_hook_slot *meego_algorithm_base_get_hook_slot(meego_algorithm_base *b, const char *algorithm_identifier) {
    meego_algorithm_hook_slot *slot = NULL;
    meego_algorithm_base_hook *i;

    pa_assert(b);
    pa_assert(algorithm_identifier);

    PA_LLIST_FOREACH(i, b->algorithm_hooks) {
        if (pa_streq(i->identifier, algorithm_identifier)) {
            if (i->enabled)
                slot = i->algorithm_hook_slot;
            break;
        }
    }

    return slot;
}
