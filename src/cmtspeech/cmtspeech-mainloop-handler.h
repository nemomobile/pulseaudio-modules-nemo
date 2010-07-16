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

#ifndef _CMTSPEECH_MAINLOOP_HANDLER_H_
#define _CMTSPEECH_MAINLOOP_HANDLER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/msgobject.h>
#include "module-meego-cmtspeech.h"

typedef struct cmtspeech_mainloop_handler {
    pa_msgobject parent;
    struct userdata *u;
} cmtspeech_mainloop_handler;

PA_DECLARE_PUBLIC_CLASS(cmtspeech_mainloop_handler);
#define CMTSPEECH_MAINLOOP_HANDLER(o) cmtspeech_mainloop_handler_cast(o)

enum {
    CMTSPEECH_MAINLOOP_HANDLER_CREATE_STREAMS,
    CMTSPEECH_MAINLOOP_HANDLER_DELETE_STREAMS,
    CMTSPEECH_MAINLOOP_HANDLER_CMT_UL_CONNECT,
    CMTSPEECH_MAINLOOP_HANDLER_CMT_UL_DISCONNECT,
    CMTSPEECH_MAINLOOP_HANDLER_CMT_DL_CONNECT,
    CMTSPEECH_MAINLOOP_HANDLER_CMT_DL_DISCONNECT,
    CMTSPEECH_MAINLOOP_HANDLER_MESSAGE_MAX
};

pa_msgobject *cmtspeech_mainloop_handler_new(struct userdata *u);

#endif
