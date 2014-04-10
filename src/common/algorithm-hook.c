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
#include <pulsecore/aupdate.h>

#include "algorithm-hook.h"

#define ALGORITHM_API_IDENTIFIER "meego-algorithm-hook-1"

struct meego_algorithm_hook_api {
    PA_REFCNT_DECLARE;

    pa_core *core;
    /* Algorithm hooks stored by string-key "name" */
    pa_hashmap *hooks;
    /* List of algorithm hooks that were deleted while
     * still having connected hook slots. */
    PA_LLIST_HEAD(meego_algorithm_hook, dead_hooks);
};

struct meego_algorithm_hook {
    meego_algorithm_hook_api *api;

    char *name;             /* Name of the hook, used as identifier when connecting slots. */
    bool enabled;           /* Hook enabled state, if all slots are disabled, hook is disabled. */
    bool dead;              /* Dead hooks are hooks that are removed, but had slots
                             * connected to them at that time. Removed at _unref() */

    pa_aupdate *aupdate;
    meego_algorithm_hook_slot *slots[2];

    /* Hooks are llist type, to be able to add to dead_hooks list. */
    PA_LLIST_FIELDS(meego_algorithm_hook);
};

/* Hook slots are stored twice for aupdate. To keep both lists identical and to be able
 * to work with correct list when doing operations to meego_algorithm_hook_slot pointer
 * (remember, caller might use slot from list that is currently not in use!) correct
 * slot is identified by id that is identical to both structs. */
struct meego_algorithm_hook_slot {
    meego_algorithm_hook *hook;     /* Hook this slot is connected to. */
    unsigned id;                    /* Slots are identified by order number, when doing enabled/free operations.
                                     * This id changes if list changes. */
    bool enabled;                   /* Enabled state of slot, disabled slots aren't fired in _fire(). */
    pa_hook_priority_t priority;    /* Slots are ordered in llist by rising priority value. */
    pa_hook_cb_t callback;          /* Slot callback */
    void *userdata;

    PA_LLIST_FIELDS(meego_algorithm_hook_slot);
};

static meego_algorithm_hook_api *algorithm_hook_new(pa_core *c) {
    meego_algorithm_hook_api *a;

    pa_assert(c);

    a = pa_xnew0(meego_algorithm_hook_api, 1);
    PA_REFCNT_INIT(a);
    a->core = c;
    a->hooks = pa_hashmap_new(pa_idxset_string_hash_func,
                              pa_idxset_string_compare_func);
    PA_LLIST_HEAD_INIT(meego_algorithm_hook, a->dead_hooks);

    pa_assert_se(pa_shared_set(c, ALGORITHM_API_IDENTIFIER, a) >= 0);

    return a;
}

meego_algorithm_hook_api *meego_algorithm_hook_api_get(pa_core *c) {
    meego_algorithm_hook_api *a;

    if ((a = pa_shared_get(c, ALGORITHM_API_IDENTIFIER)))
        return meego_algorithm_hook_api_ref(a);

    return algorithm_hook_new(c);
}

meego_algorithm_hook_api *meego_algorithm_hook_api_ref(meego_algorithm_hook_api *a) {
    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);

    PA_REFCNT_INC(a);

    return a;
}

/* Must be called with write lock */
static void slot_free(meego_algorithm_hook_slot **list, meego_algorithm_hook_slot *slot) {
    pa_assert(slot);
    pa_assert(slot->hook);
    pa_assert(list);

    PA_LLIST_REMOVE(meego_algorithm_hook_slot, *list, slot);
    pa_xfree(slot);
}

/* Must be called with write lock */
static void reset_ids(meego_algorithm_hook_slot *list) {
    meego_algorithm_hook_slot *slot;
    unsigned i;

    i = 0;
    PA_LLIST_FOREACH(slot, list)
        slot->id = i++;
}

/* Must be called with read or write lock */
static meego_algorithm_hook_slot *find_slot(meego_algorithm_hook_slot *list, unsigned id) {
    pa_assert(list);

    while (list) {
        if (id == list->id)
            break;
        list = list->next;
    }

    /* If list is now NULL it means slot with id wasn't in the list. It should be. */
    pa_assert(list);

    return list;
}

static void algorithm_hook_free(meego_algorithm_hook *hook) {
    meego_algorithm_hook_slot *slot;
    unsigned j;

    pa_assert(hook);
    pa_assert(hook->name);

    j = pa_aupdate_write_begin(hook->aupdate);

    while ((slot = hook->slots[j]))
        slot_free(&hook->slots[j], slot);

    j = pa_aupdate_write_swap(hook->aupdate);

    while ((slot = hook->slots[j]))
        slot_free(&hook->slots[j], slot);

    pa_aupdate_write_end(hook->aupdate);

    pa_aupdate_free(hook->aupdate);
    pa_xfree(hook->name);
    pa_xfree(hook);
}

void meego_algorithm_hook_api_unref(meego_algorithm_hook_api *a) {
    meego_algorithm_hook *hook;

    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);

    if (PA_REFCNT_DEC(a) > 0)
        return;

    pa_assert_se(pa_shared_remove(a->core, ALGORITHM_API_IDENTIFIER) >= 0);

    pa_hashmap_free(a->hooks, (pa_free_cb_t) algorithm_hook_free);

    /* clean up dead hooks */
    while ((hook = a->dead_hooks)) {
        PA_LLIST_REMOVE(meego_algorithm_hook, a->dead_hooks, hook);
        algorithm_hook_free(hook);
    }

    pa_xfree(a);
}

static meego_algorithm_hook* hook_new(meego_algorithm_hook_api *a, const char *name) {
    meego_algorithm_hook *hook;

    pa_assert(a);
    pa_assert(name);

    hook = pa_xnew0(meego_algorithm_hook, 1);
    hook->api = a;
    hook->name = pa_xstrdup(name);
    hook->aupdate = pa_aupdate_new();
    hook->enabled = false;
    hook->dead = false;
    PA_LLIST_HEAD_INIT(meego_algorithm_hook_slot, hook->slots[0]);
    PA_LLIST_HEAD_INIT(meego_algorithm_hook_slot, hook->slots[1]);
    PA_LLIST_INIT(meego_algorithm_hook, hook);

    return hook;
}

meego_algorithm_hook *meego_algorithm_hook_init(meego_algorithm_hook_api *a, const char *name) {
    meego_algorithm_hook *hook;

    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);
    pa_assert(name);

    if (pa_hashmap_get(a->hooks, name)) {
        pa_log_warn("meego_algorithm_hook_init: Algorithm with name \"%s\" already registered.", name);
        return NULL;
    }

    hook = hook_new(a, name);
    pa_hashmap_put(a->hooks, hook->name, hook);

    return hook;
}

void meego_algorithm_hook_done(meego_algorithm_hook *hook) {
    bool done = true;
    unsigned j;

    pa_assert(hook);
    pa_assert(hook->name);
    pa_assert(hook->api);
    pa_assert(PA_REFCNT_VALUE(hook->api) >= 1);

    hook->dead = true;
    pa_hashmap_remove(hook->api->hooks, hook->name);

    /* Check from both copies if there are still slots connected */

    j = pa_aupdate_write_begin(hook->aupdate);

    if (hook->slots[j])
        done = false;

    j = pa_aupdate_write_swap(hook->aupdate);

    if (hook->slots[j])
        done = false;

    pa_aupdate_write_end(hook->aupdate);

    /* if there are still hook_slots connected to our hook
     * we cannot clean up the hook yet. we'll add this hook to
     * dead_hooks list and clean up when meego_algorithm_hook_api struct is
     * cleaned up in meego_algorithm_hook_api_unref. */
    if (done)
        algorithm_hook_free(hook);
    else
        PA_LLIST_PREPEND(meego_algorithm_hook, hook->api->dead_hooks, hook);
}

pa_hook_result_t meego_algorithm_hook_fire(meego_algorithm_hook *hook, void *data) {
    meego_algorithm_hook_slot *slot;
    pa_hook_result_t result = PA_HOOK_OK;
    unsigned j;

    pa_assert_fp(hook);
    pa_assert_fp(hook->aupdate);
    pa_assert_fp(!hook->dead);

    j = pa_aupdate_read_begin(hook->aupdate);

    /* Go through algorithm hook slots in priority order and fire hook slot
     * callback for enabled ones. */
    PA_LLIST_FOREACH(slot, hook->slots[j]) {

        if (!slot->enabled)
            continue;

        if ((result = slot->callback(hook->api->core, data, slot->userdata)) != PA_HOOK_OK)
            break;
    }

    pa_aupdate_read_end(hook->aupdate);

    return result;
}

static meego_algorithm_hook_slot *slot_new(meego_algorithm_hook *hook, pa_hook_priority_t prio, pa_hook_cb_t cb, void *data) {
    meego_algorithm_hook_slot *slot;

    pa_assert(hook);
    pa_assert(cb);

    slot = pa_xnew0(meego_algorithm_hook_slot, 1);
    slot->hook = hook;
    slot->id = 0;
    slot->priority = prio;
    slot->callback = cb;
    slot->userdata = data;
    slot->enabled = false;
    PA_LLIST_INIT(meego_algorithm_hook_slot, slot);

    return slot;
}

/* Must be called with write lock */
static void list_add(meego_algorithm_hook_slot **list, meego_algorithm_hook_slot *slot) {
    meego_algorithm_hook_slot *prev, *where;

    /* Order extended hook slots by priority */
    prev = NULL;
    for (where = *list; where; where = where->next) {
        if (slot->priority < where->priority)
            break;
        prev = where;
    }

    PA_LLIST_INSERT_AFTER(meego_algorithm_hook_slot, *list, prev, slot);
}

meego_algorithm_hook_slot *meego_algorithm_hook_connect(meego_algorithm_hook_api *a, const char *name, pa_hook_priority_t prio, pa_hook_cb_t cb, void *data) {
    meego_algorithm_hook *hook;
    meego_algorithm_hook_slot *slot, *slot2;
    unsigned j;

    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);
    pa_assert(name);
    pa_assert(cb);

    slot = NULL;

    if ((hook = pa_hashmap_get(a->hooks, name)) && !hook->dead) {

        j = pa_aupdate_write_begin(hook->aupdate);

        slot = slot_new(hook, prio, cb, data);
        list_add(&hook->slots[j], slot);
        /* Recalculate slot ids after list order has changed. */
        reset_ids(hook->slots[j]);

        j = pa_aupdate_write_swap(hook->aupdate);

        slot2 = slot_new(hook, prio, cb, data);
        list_add(&hook->slots[j], slot2);
        /* Recalculate slot ids after list order has changed. */
        reset_ids(hook->slots[j]);

        pa_aupdate_write_end(hook->aupdate);

        pa_log_debug("Connected hook slot %u to %s", slot->id, hook->name);
    } else
        pa_log_debug("No hook with name %s registered.", name);

    return slot;
}

void meego_algorithm_hook_slot_free(meego_algorithm_hook_slot *slot) {
    meego_algorithm_hook *hook;
    unsigned id;
    unsigned j;

    pa_assert(slot);
    pa_assert(slot->hook);

    j = pa_aupdate_write_begin(slot->hook->aupdate);

    hook = slot->hook;
    id = slot->id;

    slot = find_slot(hook->slots[j], id);
    slot_free(&hook->slots[j], slot);
    /* Recalculate slot ids after list order has changed. */
    reset_ids(hook->slots[j]);

    pa_log_debug("Disconnect hook slot %u from %s", id, hook->name);

    j = pa_aupdate_write_swap(hook->aupdate);

    slot = find_slot(hook->slots[j], id);
    slot_free(&hook->slots[j], slot);
    /* Recalculate slot ids after list order has changed. */
    reset_ids(hook->slots[j]);

    pa_aupdate_write_end(hook->aupdate);
}

void meego_algorithm_hook_slot_set_enabled(meego_algorithm_hook_slot *slot, bool enabled) {
    meego_algorithm_hook_slot *s;
    unsigned j;
    bool hook_enabled = false;

    pa_assert(slot);
    pa_assert(slot->hook);

    j = pa_aupdate_write_begin(slot->hook->aupdate);

    slot = find_slot(slot->hook->slots[j], slot->id);
    slot->enabled = enabled;

    /* If any of the slots is enabled, hook is enabled.
     * If all slots are disabled, hook is disabled. */
    PA_LLIST_FOREACH(s, slot->hook->slots[j])
        if (s->enabled) {
            hook_enabled = true;
            break;
        }

    if (slot->hook->enabled != hook_enabled)
        pa_log_debug("Hook %s state changes to %s", slot->hook->name, hook_enabled ? "enabled" : "disabled");
    slot->hook->enabled = hook_enabled;

    /* Update copy as well */
    j = pa_aupdate_write_swap(slot->hook->aupdate);

    slot = find_slot(slot->hook->slots[j], slot->id);
    slot->enabled = enabled;

    pa_aupdate_write_end(slot->hook->aupdate);
}

bool meego_algorithm_hook_slot_enabled(meego_algorithm_hook_slot *slot) {
    bool enabled;
    unsigned j;

    pa_assert(slot);
    pa_assert(slot->hook);

    j = pa_aupdate_read_begin(slot->hook->aupdate);

    /* Search and select active slot */
    slot = find_slot(slot->hook->slots[j], slot->id);
    enabled = slot->enabled;

    pa_aupdate_read_end(slot->hook->aupdate);

    return enabled;
}

bool meego_algorithm_hook_enabled(meego_algorithm_hook *hook) {
    pa_assert(hook);

    return hook->enabled;
}
