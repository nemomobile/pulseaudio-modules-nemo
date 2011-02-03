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

struct meego_algorithm_base_hook {
    char *parameter_name;
    char *hook_name;
    pa_hook_priority_t priority;
    pa_hook_cb_t cb;
    meego_algorithm_hook_slot *algorithm_hook_slot;

    PA_LLIST_FIELDS(meego_algorithm_base_hook);
};

static void free_hooks(meego_algorithm_base_hook *list) {
    meego_algorithm_base_hook *c = NULL;

    while (list) {
        PA_LLIST_REMOVE(meego_algorithm_base_hook, list, c);
        if (c->algorithm_hook_slot)
            meego_algorithm_hook_slot_free(c->algorithm_hook_slot);
        if (c->parameter_name)
            pa_xfree(c->parameter_name);
        if (c->hook_name)
            pa_xfree(c->hook_name);
        pa_xfree(c);
        c = NULL;
    }
}

static int parse_hook_names(meego_algorithm_base *b,
                            const meego_algorithm_callback_list *parameter_list,
                            const meego_algorithm_callback_list *algorithm_list) {
    const meego_algorithm_callback_list *i;
    meego_algorithm_base_hook *c;

    pa_assert(b);

    if (parameter_list) {
        for (i = parameter_list; i && i->name; i++) {
            c = pa_xnew0(meego_algorithm_base_hook, 1);
            PA_LLIST_INIT(meego_algorithm_base_hook, c);

            c->parameter_name = NULL;
            c->hook_name = pa_xstrdup(i->name);
            c->priority = i->priority;
            c->cb = i->cb;

            PA_LLIST_PREPEND(meego_algorithm_base_hook, b->parameter_hooks, c);
            pa_log_debug("Add hook for %s parameters.", c->hook_name);
        }
    }

    if (algorithm_list) {
        for (i = algorithm_list; i && i->name; i++) {
            c = pa_xnew0(meego_algorithm_base_hook, 1);
            PA_LLIST_INIT(meego_algorithm_base_hook, c);

            c->parameter_name = NULL;
            c->hook_name = pa_xstrdup(i->name);
            c->priority = i->priority;
            c->cb = i->cb;

            PA_LLIST_PREPEND(meego_algorithm_base_hook, b->algorithm_hooks, c);
            pa_log_debug("Add algorithm hook %s", c->hook_name);
        }
    }

    return 0;
}

static int parse_hook_names_dynamic(meego_algorithm_base *b, pa_module *m,
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
        for (i = parameter_list; i && i->name; i++) {
            c = pa_xnew0(meego_algorithm_base_hook, 1);
            PA_LLIST_INIT(meego_algorithm_base_hook, c);

            c->parameter_name = pa_sprintf_malloc("parameter_hook_%s", i->name);
            c->priority = i->priority;
            c->cb = i->cb;

            PA_LLIST_PREPEND(meego_algorithm_base_hook, b->parameter_hooks, c);
            pa_log_debug("Add parameter with argument %s", c->parameter_name);
            argument_count++;
        }
    }

    if (algorithm_list) {
        for (i = algorithm_list; i && i->name; i++) {
            c = pa_xnew0(meego_algorithm_base_hook, 1);
            PA_LLIST_INIT(meego_algorithm_base_hook, c);

            c->parameter_name = pa_sprintf_malloc("algorithm_hook_%s", i->name);
            c->priority = i->priority;
            c->cb = i->cb;

            PA_LLIST_PREPEND(meego_algorithm_base_hook, b->algorithm_hooks, c);
            pa_log_debug("Add algorithm with argument %s", c->parameter_name);
            argument_count++;
        }
    }

    if (argument_count == 0) {
        pa_log_error("No parameter or algorithm hooks defined in implementor.");
        goto fail;
    }

    valid_modargs = pa_xnew0(const char*, argument_count+1);
    valid_modargs[argument_count] = NULL;

    PA_LLIST_FOREACH(c, b->parameter_hooks)
        valid_modargs[arg++] = c->parameter_name;

    PA_LLIST_FOREACH(c, b->algorithm_hooks)
        valid_modargs[arg++] = c->parameter_name;

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments.");
        goto fail;
    }

    PA_LLIST_FOREACH(c, b->parameter_hooks) {
        const char *v;
        if (!(v = pa_modargs_get_value(ma, c->parameter_name, NULL))) {
            pa_log("Missing argument for parameter hook %s", c->parameter_name);
            goto fail;
        }
        c->hook_name = pa_xstrdup(v);
    }

    PA_LLIST_FOREACH(c, b->algorithm_hooks) {
        const char *v;
        if (!(v = pa_modargs_get_value(ma, c->parameter_name, NULL))) {
            pa_log("Missing argument for algorithm hook %s", c->parameter_name);
            goto fail;
        }
        c->hook_name = pa_xstrdup(v);
    }

    pa_xfree(valid_modargs);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    if (valid_modargs)
        pa_xfree(valid_modargs);

    free_hooks(b->parameter_hooks);
    free_hooks(b->algorithm_hooks);

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

static meego_algorithm_base *init_base(pa_bool_t dynamic,
                                       pa_module *m,
                                       const meego_algorithm_callback_list *parameter_list,
                                       const meego_algorithm_callback_list *algorithm_list,
                                       void *userdata) {
    meego_algorithm_base *b;
    meego_algorithm_base_hook *i;

    pa_assert(m);

    b = new_base(m, userdata);

    if (dynamic) {
        if (parse_hook_names_dynamic(b, m, parameter_list, algorithm_list)) {
            pa_log_error("Failed to parse dynamic hook names.");
            goto fail;
        }
    } else {
        if (parse_hook_names(b, parameter_list, algorithm_list)) {
            pa_log_error("Failed to parse hook names.");
            goto fail;
        }
    }

    if (b->parameter_hooks) {
        PA_LLIST_FOREACH(i, b->parameter_hooks) {
            pa_log_debug("Request parameter updates for %s", i->hook_name);
            meego_parameter_request_updates(i->hook_name, i->cb, i->priority, FALSE, b->userdata);
        }
    }

    if (b->algorithm_hooks) {
        b->algorithm = meego_algorithm_hook_api_get(b->core);
        if (!b->algorithm) {
            pa_log("Failed to get algorithm interface.");
            goto fail;
        }

        PA_LLIST_FOREACH(i, b->algorithm_hooks) {
            i->algorithm_hook_slot = meego_algorithm_hook_connect(b->algorithm, i->hook_name, i->priority, i->cb, b->userdata);
            if (i->algorithm_hook_slot)
                pa_log_debug("Connected to algorithm hook %s", i->hook_name);
            else
                pa_log_error("Failed to connect to algorithm hook %s", i->hook_name);
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

meego_algorithm_base* meego_algorithm_base_init_dynamic(pa_module *m,
                                                        const meego_algorithm_callback_list *parameter_list,
                                                        const meego_algorithm_callback_list *algorithm_list,
                                                        void *userdata) {
    return init_base(TRUE, m, parameter_list, algorithm_list, userdata);
}

meego_algorithm_base* meego_algorithm_base_init(pa_module *m,
                                                const meego_algorithm_callback_list *parameter_list,
                                                const meego_algorithm_callback_list *algorithm_list,
                                                void *userdata) {
    return init_base(FALSE, m, parameter_list, algorithm_list, userdata);
}

void meego_algorithm_base_done(meego_algorithm_base *b) {
    pa_assert(b);

    free_hooks(b->algorithm_hooks);
    free_hooks(b->parameter_hooks);

    if (b->algorithm)
        meego_algorithm_hook_api_unref(b->algorithm);

    pa_xfree(b);
}
