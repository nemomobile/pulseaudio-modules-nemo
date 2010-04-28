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
 * pa_hook pointers received from algorithm_hook_init should be cleared
 * using algorithm_hook_done.
 */

#include <pulsecore/hook-list.h>

typedef struct algorithm_hook algorithm_hook;

/* get pointer to opaque algorithm_hook struct.
 * unref after use. */
algorithm_hook *algorithm_hook_get(pa_core *core);
algorithm_hook *algorithm_hook_ref(algorithm_hook *a);
void algorithm_hook_unref(algorithm_hook *a);

/* init new hook with name. hook_data is pointer to pulseaudio
 * core struct.
 * returns pointer to newly initialized hook on success, on error
 * returns NULL */
pa_hook *algorithm_hook_init(algorithm_hook *a, const char *name);
/* clear hook with name. */
pa_bool_t algorithm_hook_done(algorithm_hook *a, const char *name);

/* connect to hook with name. if no hook is initialized with
 * given name, returns NULL. */
pa_hook_slot *algorithm_hook_connect(algorithm_hook *a, const char *name, pa_hook_priority_t prio, pa_hook_cb_t cb, void *data);

#endif
