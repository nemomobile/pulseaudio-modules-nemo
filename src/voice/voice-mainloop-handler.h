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

#ifndef _VOICE_MAINLOOP_HANDLER_H_
#define _VOICE_MAINLOOP_HANDLER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/msgobject.h>
#include "module-voice-userdata.h"

typedef struct voice_mainloop_handler {
    pa_msgobject parent;
    struct userdata *u;
} voice_mainloop_handler;

PA_DECLARE_PUBLIC_CLASS(voice_mainloop_handler);
#define VOICE_MAINLOOP_HANDLER(o) voice_mainloop_handler_cast(o)

enum {
    VOICE_MAINLOOP_HANDLER_EXECUTE,
    VOICE_MAINLOOP_HANDLER_MESSAGE_MAX
};

typedef struct voice_mainloop_handler_execute {
    int (*execute)(struct userdata *u, void *parameter);
    void *parameter;
} voice_mainloop_handler_execute;

pa_msgobject *voice_mainloop_handler_new(struct userdata *u);

#endif
