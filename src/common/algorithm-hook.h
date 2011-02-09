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
#ifndef _algorithm_hook_h_
#define _algorithm_hook_h_

/* Many modules may need to run audio enhancement etc algorithms on
 * audio data. Algorithm hook provides a mechanism to separate normal
 * module functions and the actual algorithm code from each other.
 * Module requiring processing should define it's hooks names and
 * export those for modules that wish to implement the processing.
 *
 * For example module-meego-music defines two hooks, and uses
 * aglorithm_hook_init to register those hooks. After hooks are registered
 * and module-meego-music receives pointer to created hook,
 * module-meego-music can use normal hook firing defined in
 * pulsecore/hook-list.h.
 *
 * If a module wishes to implement processing, it needs to call
 * algorithm_hook_connect with a name that has already been registered
 * (for example by module-meego-music). After this, whenever
 * module-meego-music fires the hook, algorithm implementer's callback
 * is called with the data passed when firing.
 *
 * Algorithm implementors should be created so that modules firing the
 * hooks don't need to know anything implementation specific, ie.
 * modules can safely fire hooks that are connected or not.
 *
 * pa_hook_slot_free should be used for pa_hook_slot pointer,
 * pa_hook pointers received from meego_algorithm_hook_init should be cleared
 * using algorithm_hook_done.
 */

#include <stdint.h>
#include <pulsecore/core.h>
#include <pulsecore/hook-list.h>
#include <pulsecore/memchunk.h>

#define MEEGO_ALGORITHM_HOOK_CHANNELS_MAX (8)

typedef struct meego_algorithm_hook_api meego_algorithm_hook_api;
typedef struct meego_algorithm_hook meego_algorithm_hook;
typedef struct meego_algorithm_hook_slot meego_algorithm_hook_slot;
typedef struct meego_algorithm_hook_data meego_algorithm_hook_data;

struct meego_algorithm_hook_data {
    uint8_t channels;
    pa_memchunk *channel[MEEGO_ALGORITHM_HOOK_CHANNELS_MAX];
};

/* get pointer to opaque algorithm_hook struct.
 * unref after use. */
meego_algorithm_hook_api *meego_algorithm_hook_api_get(pa_core *core);
meego_algorithm_hook_api *meego_algorithm_hook_api_ref(meego_algorithm_hook_api *a);
void meego_algorithm_hook_api_unref(meego_algorithm_hook_api *a);

/* init new hook with name. hook_data is pointer to pulseaudio
 * core struct.
 * returns pointer to newly initialized hook on success, on error
 * returns NULL */
/* Increases hook_api reference counter */
meego_algorithm_hook *meego_algorithm_hook_init(meego_algorithm_hook_api *a, const char *name);
/* clear hook with name. */
/* Decreases hook_api reference counter */
pa_bool_t meego_algorithm_hook_done(meego_algorithm_hook *hook);

pa_hook_result_t meego_algorithm_hook_fire(meego_algorithm_hook *hook, void *data);

pa_bool_t pa_algorithm_hook_is_firing(meego_algorithm_hook *hook);

/* connect to hook with name. if no hook is initialized with
 * given name, returns NULL. */
/* Increases hook_api reference counter */
meego_algorithm_hook_slot *meego_algorithm_hook_connect(meego_algorithm_hook_api *a, const char *name, pa_hook_priority_t prio, pa_hook_cb_t cb, void *data);
/* Decreases hook_api reference counter */
void meego_algorithm_hook_slot_free(meego_algorithm_hook_slot *slot);

void meego_algorithm_hook_slot_set_enabled(meego_algorithm_hook_slot *slot, pa_bool_t enabled);
pa_bool_t meego_algorithm_hook_slot_enabled(meego_algorithm_hook_slot *slot);
pa_bool_t meego_algorithm_hook_enabled(meego_algorithm_hook *hook);


#endif
