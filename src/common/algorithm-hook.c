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

struct algorithm_hook {
    PA_REFCNT_DECLARE;

    pa_core *core;
    pa_hashmap *hooks;
};

typedef struct algorithm_entry {
    char *name;
    pa_hook hook;
    pa_bool_t dead;
} algorithm_entry;

static algorithm_hook *algorithm_hook_new(pa_core *c) {
    algorithm_hook *a;

    pa_assert(c);

    a = pa_xnew0(algorithm_hook, 1);
    PA_REFCNT_INIT(a);
    a->core = pa_core_ref(c);
    a->hooks = pa_hashmap_new(pa_idxset_string_hash_func,
                              pa_idxset_string_compare_func);

    pa_assert_se(pa_shared_set(c, "algorithm-hook-0", a) >= 0);

    return a;
}

algorithm_hook *algorithm_hook_get(pa_core *c) {
    algorithm_hook *a;

    if ((a = pa_shared_get(c, "algorithm-hook-0")))
        return algorithm_hook_ref(a);

    return algorithm_hook_new(c);
}

algorithm_hook *algorithm_hook_ref(algorithm_hook *a) {
    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);

    PA_REFCNT_INC(a);

    return a;
}

static void algorithm_entry_free(algorithm_entry *e, void *userdata) {
    pa_assert(e);
    pa_assert(e->name);

    pa_xfree(e->name);
    pa_xfree(e);
}

void algorithm_hook_unref(algorithm_hook *a) {
    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);

    if (PA_REFCNT_DEC(a) > 0)
        return;

    pa_assert_se(pa_shared_remove(a->core, "algorithm-hook-0") >= 0);

    pa_hashmap_free(a->hooks, (pa_free2_cb_t)algorithm_entry_free, NULL);

    pa_core_unref(a->core);

    pa_xfree(a);
}

pa_hook *algorithm_hook_init(algorithm_hook *a, const char *name) {
    algorithm_entry *entry;

    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);
    pa_assert(name);

    if (!(entry = pa_hashmap_get(a->hooks, name))) {
        entry = pa_xnew0(algorithm_entry, 1);
        entry->name = pa_xstrdup(name);
        entry->dead = FALSE;
        pa_hook_init(&entry->hook, a->core);
        pa_hashmap_put(a->hooks, entry->name, entry);
        return &entry->hook;
    } else {
        pa_log_warn("algorithm_hook_register: Algorithm with name \"%s\" already registered.", name);
        return NULL;
    }
}

pa_bool_t algorithm_hook_done(algorithm_hook *a, const char *name) {
    algorithm_entry *entry;

    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);
    pa_assert(name);

    if ((entry = pa_hashmap_get(a->hooks, name))) {
        /* if there are still hook_slots connected to our hook
         * we cannot clean up the hook yet. we'll mark this entry as
         * dead and clean up when algorithm_hook struct is
         * cleaned up in algorithm_hook_unref. */
        if (!entry->hook.slots) {
            pa_hook_done(&entry->hook);
            pa_hashmap_remove(a->hooks, entry->name);
            algorithm_entry_free(entry, NULL);
        } else {
            entry->dead = TRUE;
        }
        return TRUE;
    } else {
        pa_log_warn("algorithm_hook_unregister: Couldn't unregister algorithm with name \"%s\": doesn't exist.", name);
        return FALSE;
    }
}

pa_hook_slot *algorithm_hook_connect(algorithm_hook *a, const char *name, pa_hook_priority_t prio, pa_hook_cb_t cb, void *data) {
    algorithm_entry *entry;
    pa_hook_slot *slot;

    pa_assert(a);
    pa_assert(PA_REFCNT_VALUE(a) >= 1);
    pa_assert(name);
    pa_assert(cb);

    slot = NULL;

    if ((entry = pa_hashmap_get(a->hooks, name))) {
        if (!entry->dead)
            slot = pa_hook_connect(&entry->hook, prio, cb, data);
    }

    return slot;
}


