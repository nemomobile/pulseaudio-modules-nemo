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

#include <pulsecore/core.h>
#include <pulsecore/hook-list.h>
#include <pulsecore/log.h>
#include <pulsecore/macro.h>
#include <pulsecore/refcnt.h>
#include <pulsecore/shared.h>
#include <pulsecore/hashmap.h>
#include <pulsecore/idxset.h>

#include "algorithm-hook.h"

struct meego_algorithm_hook_api {
    PA_REFCNT_DECLARE;

    pa_core *core;
    pa_hashmap *hooks;
};

struct meego_algorithm_hook {
    meego_algorithm_hook_api *api;

    char *name;
    pa_hook hook;
    pa_bool_t initialized;
    pa_bool_t enabled;
    pa_bool_t dead;
};

struct meego_algorithm_hook_slot {
    meego_algorithm_hook_api *api;
    meego_algorithm_hook *hook;

    pa_hook_slot *hook_slot;
};

static meego_algorithm_hook_api *algorithm_hook_new(pa_core *c) {
    meego_algorithm_hook_api *a;

    pa_assert(c);

    a = pa_xnew0(meego_algorithm_hook_api, 1);
    PA_REFCNT_INIT(a);
    a->core = pa_core_ref(c);
    a->hooks = pa_hashmap_new(pa_idxset_string_hash_func,
                              pa_idxset_string_compare_func);

    pa_assert_se(pa_shared_set(c, "algorithm-hook-0", a) >= 0);

    return a;
}

meego_algorithm_hook_api *meego_algorithm_hook_api_get(pa_core *c) {
    meego_algorithm_hook_api *a;

    if ((a = pa_shared_get(c, "algorithm-hook-0")))
        return meego_algorithm_hook_api_ref(a);

    return algorithm_hook_new(c);
}

meego_algorithm_hook_api *meego_algorithm_hook_api_ref(meego_algorithm_hook_api *a) {
    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);

    PA_REFCNT_INC(a);

    return a;
}

static void algorithm_hook_free(meego_algorithm_hook *hook, void *userdata) {
    pa_assert(hook);
    pa_assert(hook->name);

    pa_xfree(hook->name);
    pa_xfree(hook);
}

void meego_algorithm_hook_api_unref(meego_algorithm_hook_api *a) {
    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);

    if (PA_REFCNT_DEC(a) > 0)
        return;

    pa_assert_se(pa_shared_remove(a->core, "algorithm-hook-0") >= 0);

    pa_hashmap_free(a->hooks, (pa_free2_cb_t)algorithm_hook_free, NULL);

    pa_core_unref(a->core);

    pa_xfree(a);
}

static meego_algorithm_hook* get_hook(meego_algorithm_hook_api *a, const char *name) {
    meego_algorithm_hook *hook;

    pa_assert(a);
    pa_assert(name);

    if (!(hook = pa_hashmap_get(a->hooks, name))) {
        hook = pa_xnew0(meego_algorithm_hook, 1);
        hook->api = a;
        hook->name = pa_xstrdup(name);
        hook->initialized = FALSE;
        hook->enabled = FALSE;
        hook->dead = FALSE;
        pa_hashmap_put(a->hooks, hook->name, hook);
    }

    return hook;
}

meego_algorithm_hook *meego_algorithm_hook_init(meego_algorithm_hook_api *a, const char *name) {
    meego_algorithm_hook *hook;

    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);
    pa_assert(name);

    hook = get_hook(a, name);

    /* hook with same name already initialized */
    if (hook->initialized) {
        pa_log_warn("meego_algorithm_hook_init: Algorithm with name \"%s\" already registered.", name);
        return NULL;
    }

    pa_hook_init(&hook->hook, a->core);
    hook->initialized = TRUE;
    return hook;
}

pa_bool_t meego_algorithm_hook_done(meego_algorithm_hook *hook) {
    pa_assert(hook);
    pa_assert(hook->name);
    pa_assert(hook->api);
    pa_assert(PA_REFCNT_VALUE(hook->api) >= 1);

    if (pa_hashmap_get(hook->api->hooks, hook->name)) {
        /* if there are still hook_slots connected to our hook
         * we cannot clean up the hook yet. we'll mark this hook as
         * dead and clean up when meego_algorithm_hook_api struct is
         * cleaned up in meego_algorithm_hook_api_unref. */
        if (!hook->hook.slots) {
            pa_hook_done(&hook->hook);
            pa_hashmap_remove(hook->api->hooks, hook->name);
            algorithm_hook_free(hook, NULL);
        } else {
            hook->dead = TRUE;
        }
        return TRUE;
    } else {
        pa_log_warn("meego_algorithm_hook_done: Couldn't unregister algorithm with name \"%s\": doesn't exist.", hook->name);
        return FALSE;
    }
}

pa_hook_result_t meego_algorithm_hook_fire(meego_algorithm_hook *hook, void *data) {
    pa_assert(hook);

    return pa_hook_fire(&hook->hook, data);
}

pa_bool_t pa_algorithm_hook_is_firing(meego_algorithm_hook *hook) {
    pa_assert(hook);

    return pa_hook_is_firing(&hook->hook);
}

meego_algorithm_hook_slot *meego_algorithm_hook_connect(meego_algorithm_hook_api *a, const char *name, pa_hook_priority_t prio, pa_hook_cb_t cb, void *data) {
    meego_algorithm_hook *hook;
    meego_algorithm_hook_slot *slot;

    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);
    pa_assert(name);
    pa_assert(cb);

    slot = NULL;

    if ((hook = pa_hashmap_get(a->hooks, name))) {
        if (hook->initialized && !hook->dead) {
            slot = pa_xnew0(meego_algorithm_hook_slot, 1);
            slot->api = a;
            slot->hook = hook;
            slot->hook_slot = pa_hook_connect(&hook->hook, prio, cb, data);
            meego_algorithm_hook_api_ref(a);
            pa_log_debug("Connected hook slot %p to %s", (void*) slot, hook->name);
        }
    }

    return slot;
}

void meego_algorithm_hook_slot_free(meego_algorithm_hook_slot *slot) {
    meego_algorithm_hook_api *a;

    pa_assert(slot);

    pa_log_debug("Disconnect hook slot %p from %s", (void*) slot, slot->hook->name);

    a = slot->api;
    pa_hook_slot_free(slot->hook_slot);
    pa_xfree(slot);
    meego_algorithm_hook_api_unref(a);
}

void meego_algorithm_hook_slot_set_enabled(meego_algorithm_hook_slot *slot, pa_bool_t enabled) {
    pa_assert(slot);
    pa_assert(slot->hook);

    /*slot->enabled = enabled;*/
    /*check_hook_enabled(slot->hook);*/
    /* TODO: all slots have their own enabled status, and
     * if any of those is enabled, hook is enabled also. */

    slot->hook->enabled = enabled;
}

pa_bool_t meego_algorithm_hook_slot_enabled(meego_algorithm_hook_slot *slot) {
    pa_assert(slot);
    pa_assert(slot->hook);

    return slot->hook->enabled;
}

pa_bool_t meego_algorithm_hook_enabled(meego_algorithm_hook *hook) {
    pa_assert(hook);

    return hook->enabled;
}
