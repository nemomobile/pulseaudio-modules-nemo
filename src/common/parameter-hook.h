#ifndef _parameter_hook_h_
#define _parameter_hook_h_

struct connect_args {
    const char *name;
    pa_hook_cb_t cb;
    pa_hook_priority_t prio;
    void *u;
};

struct update_args {
    void *parameters;
    unsigned length;
};

int request_parameter_updates(const char *name, pa_hook_cb_t cb, pa_hook_priority_t prio, void *u);
pa_hook_slot *receive_update_requests(pa_hook_cb_t cb, void *p);
void discontinue_update_requests(pa_hook_slot *s);

#endif
