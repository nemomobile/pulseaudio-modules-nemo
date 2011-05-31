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

#include <meego/parameter-hook.h>
#include <meego/parameter-hook-implementor.h>
#include <meego/parameter-modifier.h>

static pa_hook parameter_update_requests;
static pa_hook *parameter_update_requests_ptr = NULL;

static pa_hook parameter_stop_requests;
static pa_hook *parameter_stop_requests_ptr = NULL;

static pa_hook modifier_register_requests;
static pa_hook *modifier_register_requests_ptr = NULL;

static pa_hook modifier_unregister_requests;
static pa_hook *modifier_unregister_requests_ptr = NULL;

int meego_parameter_request_updates(const char *name, pa_hook_cb_t cb, pa_hook_priority_t prio, pa_bool_t full_updates, void *userdata) {
    meego_parameter_connection_args args;

    pa_assert(cb);

    if (!parameter_update_requests_ptr) {
        pa_log_warn("Parameter update service not available");
        return -1;
    }

    args.name = name;
    args.cb = cb;
    args.prio = prio;
    args.full_updates = full_updates;
    args.userdata = userdata;

    pa_log_debug("Requesting updates for %s", name ? name : "mode changes");

    pa_hook_fire(parameter_update_requests_ptr, &args);

    return 0;
}

int meego_parameter_stop_updates(const char *name, pa_hook_cb_t cb, void *userdata) {
    meego_parameter_connection_args args;

    pa_assert(cb);

    if (!parameter_stop_requests_ptr) {
        pa_log_warn("Parameter update service not available");
        return -1;
    }

    args.name = name;
    args.cb = cb;
    args.userdata = userdata;

    if (!name)
        pa_log_debug("Stopping mode updates");
    else
        pa_log_debug("Stopping updates for %s", name);

    pa_hook_fire(parameter_stop_requests_ptr, &args);

    return 0;
}

static pa_hook_slot* init_hook(pa_hook *hook, pa_hook **hook_ptr, pa_hook_cb_t cb, pa_core *c, void *userdata) {
    if (!*hook_ptr) {
        *hook_ptr = hook;
        pa_hook_init(*hook_ptr, c);
    }

    return pa_hook_connect(*hook_ptr, PA_HOOK_NORMAL, cb, userdata);
}


void meego_parameter_receive_requests(pa_core *c, meego_parameter_hook_implementor_args *args, void *userdata) {

    pa_assert(c);
    pa_assert(args);
    pa_assert(args->update_request_cb);
    pa_assert(args->stop_request_cb);
    pa_assert(args->modifier_registration_cb);
    pa_assert(args->modifier_unregistration_cb);
    pa_assert(!args->update_request_slot);
    pa_assert(!args->modifier_registration_slot);
    pa_assert(!args->modifier_unregistration_slot);

    args->update_request_slot = init_hook(&parameter_update_requests,
                                          &parameter_update_requests_ptr,
                                          args->update_request_cb,
                                          c,
                                          userdata);

    args->stop_request_slot = init_hook(&parameter_stop_requests,
                                        &parameter_stop_requests_ptr,
                                        args->stop_request_cb,
                                        c,
                                        userdata);

    args->modifier_registration_slot = init_hook(&modifier_register_requests,
                                                 &modifier_register_requests_ptr,
                                                 args->modifier_registration_cb,
                                                 c,
                                                 userdata);

    args->modifier_unregistration_slot = init_hook(&modifier_unregister_requests,
                                                   &modifier_unregister_requests_ptr,
                                                   args->modifier_unregistration_cb,
                                                   c,
                                                   userdata);
}

void meego_parameter_discontinue_requests(meego_parameter_hook_implementor_args *args) {

    pa_assert(args);

    if (args->update_request_slot)
        pa_hook_slot_free(args->update_request_slot);
    if (args->stop_request_slot)
        pa_hook_slot_free(args->stop_request_slot);
    if(args->modifier_registration_slot)
        pa_hook_slot_free(args->modifier_registration_slot);
    if(args->modifier_unregistration_slot)
        pa_hook_slot_free(args->modifier_unregistration_slot);
    parameter_update_requests_ptr = NULL;
    parameter_stop_requests_ptr = NULL;
    modifier_register_requests_ptr = NULL;
    modifier_unregister_requests_ptr = NULL;
}

int meego_parameter_register_modifier(meego_parameter_modifier *modifier) {

    pa_assert(modifier);
    pa_assert(modifier->mode_name);
    pa_assert(modifier->algorithm_name);
    pa_assert(modifier->get_parameters);

    if (!modifier_register_requests_ptr) {
        pa_log_warn("Parameter modifier registration service not available");
        return -1;
    }

    pa_hook_fire(modifier_register_requests_ptr, modifier);

    return 0;
}

int meego_parameter_unregister_modifier(meego_parameter_modifier *modifier) {

    pa_assert(modifier);
    pa_assert(modifier->mode_name);
    pa_assert(modifier->algorithm_name);
    pa_assert(modifier->get_parameters);

    if (!modifier_unregister_requests_ptr) {
        pa_log_warn("Parameter modifier unregistration service not available");
        return -1;
    }

    pa_hook_fire(modifier_unregister_requests_ptr, modifier);

    return 0;
}
