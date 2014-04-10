/***
  This file is copied from PulseAudio (the version used in MeeGo 1.2
  Harmattan).

  Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

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
#include <pulse/volume.h>

#include "volume-proxy.h"

struct pa_volume_proxy {
    PA_REFCNT_DECLARE;

    pa_core *core;
    pa_hashmap *volumes;
    pa_hook hooks[PA_VOLUME_PROXY_HOOK_MAX];
};

struct volume_entry {
    char *name;
    pa_volume_t volume;
};

static pa_volume_proxy* volume_proxy_new(pa_core *c) {
    pa_volume_proxy *r;
    pa_volume_proxy_hook_t h;

    pa_assert(c);

    r = pa_xnew0(pa_volume_proxy, 1);
    PA_REFCNT_INIT(r);
    r->core = c;
    r->volumes = pa_hashmap_new(pa_idxset_string_hash_func,
                                pa_idxset_string_compare_func);

    for (h = 0; h < PA_VOLUME_PROXY_HOOK_MAX; h++)
        pa_hook_init(&r->hooks[h], r);

    pa_assert_se(pa_shared_set(c, "volume-proxy", r) >= 0);

    return r;
}

pa_volume_proxy *pa_volume_proxy_get(pa_core *core) {
    pa_volume_proxy *r;

    if ((r = pa_shared_get(core, "volume-proxy")))
        return pa_volume_proxy_ref(r);

    return volume_proxy_new(core);
}

pa_volume_proxy *pa_volume_proxy_ref(pa_volume_proxy *r) {
    pa_assert(r);
    pa_assert(PA_REFCNT_VALUE(r) >= 1);

    PA_REFCNT_INC(r);

    return r;
}

static void volume_entry_free(struct volume_entry *e) {
    pa_assert(e);
    pa_assert(e->name);

    pa_xfree(e->name);
    pa_xfree(e);
}

void pa_volume_proxy_unref(pa_volume_proxy *r) {
    pa_volume_proxy_hook_t h;

    pa_assert(r);
    pa_assert(PA_REFCNT_VALUE(r) >= 1);

    if (PA_REFCNT_DEC(r) > 0)
        return;

    for (h = 0; h < PA_VOLUME_PROXY_HOOK_MAX; h++)
        pa_hook_done(&r->hooks[h]);

    pa_assert_se(pa_shared_remove(r->core, "volume-proxy") >= 0);

    pa_hashmap_free(r->volumes, (pa_free_cb_t) volume_entry_free);

    pa_xfree(r);
}

bool pa_volume_proxy_get_volume(pa_volume_proxy *r, const char *name, pa_volume_t *return_volume) {
    struct volume_entry *e;

    pa_assert(r);
    pa_assert(PA_REFCNT_VALUE(r) >= 1);
    pa_assert(return_volume);

    if ((e = pa_hashmap_get(r->volumes, name))) {
        *return_volume = e->volume;
        return true;
    }

    return false;
}

void pa_volume_proxy_set_volume(pa_volume_proxy *r, const char *name, pa_volume_t volume) {
    struct volume_entry *e;
    bool changed;

    pa_assert(r);
    pa_assert(PA_REFCNT_VALUE(r) >= 1);

    if (!(e = pa_hashmap_get(r->volumes, name))) {
        e = pa_xnew0(struct volume_entry, 1);
        e->name = pa_xstrdup(name);
        pa_hashmap_put(r->volumes, e->name, e);
        changed = true;
    } else
        changed = e->volume != volume;

    e->volume = volume;

    if (changed)
        pa_hook_fire(&r->hooks[PA_VOLUME_PROXY_HOOK_CHANGED], (void *) e->name);
}

pa_hook *pa_volume_proxy_hooks(pa_volume_proxy *r) {
    pa_assert(r);
    pa_assert(PA_REFCNT_VALUE(r) >= 1);

    return r->hooks;
}
