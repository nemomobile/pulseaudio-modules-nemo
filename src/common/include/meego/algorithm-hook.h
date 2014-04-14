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
 * Module requiring processing should define its hooks names and
 * export those for modules that wish to implement the processing.
 *
 * If a module wishes to implement processing, it needs to call
 * meego_algorithm_hook_connect() with a name that has already been registered
 * using meego_algorithm_hook_init(). After this, whenever
 * hook owner fires the hook, algorithm implementer's callback
 * is called with the data passed when firing.
 *
 * Algorithm implementors should be created so that modules firing the
 * hooks don't need to know anything implementation specific, ie.
 * modules can safely fire hooks that are connected or not.
 */

#include <stdint.h>
#include <pulsecore/core.h>
#include <pulsecore/hook-list.h>
#include <pulsecore/memchunk.h>

#define MEEGO_ALGORITHM_HOOK_CHANNELS_MAX (8)

/* Struct for algorithm hook internals. */
typedef struct meego_algorithm_hook_api meego_algorithm_hook_api;
/* Module implementor creates new hook for processing, and receives new pointer
 * to meego_algorithm_hook. This is then used when firing the hook. */
typedef struct meego_algorithm_hook meego_algorithm_hook;
/* When hook processing implementors connect to algorithm hooks, they receive
 * pointer to hook slot for changing algorithm state or disconnecting. */
typedef struct meego_algorithm_hook_slot meego_algorithm_hook_slot;

/* Default type for call_data in algorithm hook slot processing callback.
 * This may be different for your hook, exact data type is defined in hook APIs. */
typedef struct meego_algorithm_hook_data meego_algorithm_hook_data;

struct meego_algorithm_hook_data {
    uint8_t channels;
    pa_memchunk channel[MEEGO_ALGORITHM_HOOK_CHANNELS_MAX];
};


/* Get pointer to opaque meego_algorithm_hook_api struct.
 * Unref after use. */
meego_algorithm_hook_api *meego_algorithm_hook_api_get(pa_core *core);
meego_algorithm_hook_api *meego_algorithm_hook_api_ref(meego_algorithm_hook_api *a);
void meego_algorithm_hook_api_unref(meego_algorithm_hook_api *a);

/* Create a new hook with given name.
 * returns pointer to meego_algorithm_hook on success, on error
 * returns NULL */
meego_algorithm_hook *meego_algorithm_hook_init(meego_algorithm_hook_api *a, const char *name);
/* Clean up hook. */
void meego_algorithm_hook_done(meego_algorithm_hook *hook);

/* Fire hook for processing in algorithm hook implementors. It is guaranteed that all hook slots
 * that are connected to hook are in one enabled state for the duration of single hook firing. */
pa_hook_result_t meego_algorithm_hook_fire(meego_algorithm_hook *hook, void *data);

/* Connect to hook with name. Returns new meego_algorithm_hook_slot on success,
 * if no hook is initialized with given name, returns NULL.
 * Hook slot is disabled by default after connecting, so you need to change its state
 * with meego_algorithm_hook_slot_set_enabled().
 *
 * Data for algorithm processing is received in pa_hook_cb_t.
 * hook_data is pointer to pa_core.
 * call_data is pointer to data to process, default is meego_algorithm_hook_data.
 * slot_data is pointer to userdata given in meego_algorithm_hook_connect().
 */
meego_algorithm_hook_slot *meego_algorithm_hook_connect(meego_algorithm_hook_api *a, const char *name, pa_hook_priority_t prio, pa_hook_cb_t cb, void *userdata);
/* Release hook slot */
void meego_algorithm_hook_slot_free(meego_algorithm_hook_slot *slot);

/* Set hook slot enabled state. This changes hook enabled state as well, so that
 * if at least one connected hook slot is enabled, hook is also enabled. If there
 * are no connected hook slots or all connected hook slots are disabled, hook is disabled.
 * Callbacks for hook slots that are disabled won't be called when firing the hook. */
void meego_algorithm_hook_slot_set_enabled(meego_algorithm_hook_slot *slot, bool enabled);
bool meego_algorithm_hook_slot_enabled(meego_algorithm_hook_slot *slot);

/* If hook is disabled (all slots are disabled), there is no need to call
 * meego_algorithm_hook_fire() for data. */
bool meego_algorithm_hook_enabled(meego_algorithm_hook *hook);


#endif
