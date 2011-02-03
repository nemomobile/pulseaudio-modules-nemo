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

#ifndef _meego_algorithm_base_h_
#define _meego_algorithm_base_h_

#include <pulsecore/hook-list.h>

#include <meego/algorithm-hook.h>
#include <meego/parameter-hook.h>

struct meego_algorithm_callback_list {
    const char *name;
    pa_hook_priority_t priority;
    pa_hook_cb_t cb;
};

struct meego_algorithm_base_hook;
typedef struct meego_algorithm_base_hook meego_algorithm_base_hook;

struct meego_algorithm_base {
    pa_core *core;
    pa_module *module;

    meego_algorithm_hook_api *algorithm;
    PA_LLIST_HEAD(meego_algorithm_base_hook, algorithm_hooks);
    PA_LLIST_HEAD(meego_algorithm_base_hook, parameter_hooks);

    void *userdata;
};


typedef struct meego_algorithm_base meego_algorithm_base;
typedef struct meego_algorithm_callback_list meego_algorithm_callback_list;

meego_algorithm_base* meego_algorithm_base_init(pa_module *m,
                                                const meego_algorithm_callback_list *parameter_list,
                                                const meego_algorithm_callback_list *algorithm_list,
                                                void *userdata);

meego_algorithm_base* meego_algorithm_base_init_dynamic(pa_module *m,
                                                        const meego_algorithm_callback_list *parameter_list,
                                                        const meego_algorithm_callback_list *algorithm_list,
                                                        void *userdata);

void meego_algorithm_base_done(meego_algorithm_base *b);


#endif
