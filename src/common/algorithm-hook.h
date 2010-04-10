#ifndef _algorithm_hook_h_
#define _algorithm_hook_h_

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

/* get pointer to hook with name. if no hook is initialized with
 * given name, returns NULL. */
pa_hook *algorithm_hook_get_hook(algorithm_hook *a, const char *name);

#endif
