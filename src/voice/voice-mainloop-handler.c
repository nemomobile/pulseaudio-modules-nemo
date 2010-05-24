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

#include "voice-mainloop-handler.h"
#include "voice-hw-sink-input.h"
#include "voice-hw-source-output.h"

PA_DEFINE_PUBLIC_CLASS(voice_mainloop_handler, pa_msgobject);

static void handle_execute_message(struct userdata *u, voice_mainloop_handler_execute *e) {
    pa_assert(u);
    pa_assert(e);
    pa_assert(e->execute);
    e->execute(u, e->parameter);
}

static void mainloop_handler_free(pa_object *o) {
    voice_mainloop_handler *h = VOICE_MAINLOOP_HANDLER(o);
    pa_log_info("Free called");
    pa_xfree(h);
}

static int mainloop_handler_process_msg(pa_msgobject *o, int code, void *userdata, int64_t offset, pa_memchunk *chunk) {
    voice_mainloop_handler *h = VOICE_MAINLOOP_HANDLER(o);
    struct userdata *u;

    voice_mainloop_handler_assert_ref(h);
    pa_assert_se(u = h->u);

    switch (code) {

        /* This is here only as on example. If you need to delegate something
           to maithread from IO-thread, add a new message. */
    case VOICE_MAINLOOP_HANDLER_EXECUTE:
        pa_log_debug("Got execute message !");
        handle_execute_message(h->u, (voice_mainloop_handler_execute *) userdata);
        return 0;

    case VOICE_MAINLOOP_HANDLER_MESSAGE_MAX:
    default:
        pa_log_error("Unknown message code %d", code);
        return -1;
    }
}

pa_msgobject *voice_mainloop_handler_new(struct userdata *u) {
    voice_mainloop_handler *h;
    pa_assert(u);
    pa_assert(u->core);
    pa_assert_se(h = pa_msgobject_new(voice_mainloop_handler));
    h->parent.parent.free = mainloop_handler_free;
    h->parent.process_msg = mainloop_handler_process_msg;
    h->u = u;
    return (pa_msgobject *)h;
}
