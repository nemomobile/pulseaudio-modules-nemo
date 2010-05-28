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

#include "cmtspeech-mainloop-handler.h"
#include <pulsecore/namereg.h>
#include "proplist-meego.h"

PA_DEFINE_PUBLIC_CLASS(cmtspeech_mainloop_handler, pa_msgobject);

static void mainloop_handler_free(pa_object *o) {
    cmtspeech_mainloop_handler *h = CMTSPEECH_MAINLOOP_HANDLER(o);
    pa_log_info("Free called");
    pa_xfree(h);
}

#define PA_ALSA_SOURCE_PROP_BUFFERS "x-maemo.alsa_source.buffers"
#define PA_ALSA_PROP_BUFFERS_PRIMARY "primary"
#define PA_ALSA_PROP_BUFFERS_ALTERNATIVE "alternative"

static void set_source_buffer_mode(struct userdata *u, const char *mode) {
    pa_source *om_source;
    pa_assert(u);

    om_source = pa_namereg_get(u->module->core, "source.hw0", PA_NAMEREG_SOURCE);

    if (om_source) {
        pa_log_debug("Setting property %s for %s to %s", PA_ALSA_SOURCE_PROP_BUFFERS,
                     om_source->name, mode);
        pa_proplist_sets(om_source->proplist, PA_ALSA_SOURCE_PROP_BUFFERS, mode);
        pa_hook_fire(&u->core->hooks[PA_CORE_HOOK_SOURCE_PROPLIST_CHANGED], om_source);
    }
    else
        pa_log_warn("Source not found");
}

static int mainloop_handler_process_msg(pa_msgobject *o, int code, void *userdata, int64_t offset, pa_memchunk *chunk) {
    cmtspeech_mainloop_handler *h = CMTSPEECH_MAINLOOP_HANDLER(o);
    struct userdata *u;

    cmtspeech_mainloop_handler_assert_ref(h);
    pa_assert_se(u = h->u);

    switch (code) {

    case CMTSPEECH_MAINLOOP_HANDLER_CMT_UL_CONNECT:
        pa_log_debug("Handling CMTSPEECH_MAINLOOP_HANDLER_CMT_UL_CONNECT");
        cmtspeech_create_source_output(u);
        return 0;

    case CMTSPEECH_MAINLOOP_HANDLER_CMT_UL_DISCONNECT:
        pa_log_debug("Handling CMTSPEECH_MAINLOOP_HANDLER_CMT_UL_DISCONNECT");
        cmtspeech_delete_source_output(u);
        return 0;

    case CMTSPEECH_MAINLOOP_HANDLER_CMT_DL_CONNECT:
        pa_log_debug("Handling CMTSPEECH_MAINLOOP_HANDLER_CMT_DL_CONNECT");
        cmtspeech_create_sink_input(u);
        return 0;

    case CMTSPEECH_MAINLOOP_HANDLER_CMT_DL_DISCONNECT:
        pa_log_debug("Handling CMTSPEECH_MAINLOOP_HANDLER_CMT_DL_DISCONNECT");
        cmtspeech_delete_sink_input(u);
        return 0;

   default:
        pa_log_error("Unknown message code %d", code);
        return -1;
    }
}

pa_msgobject *cmtspeech_mainloop_handler_new(struct userdata *u) {
    cmtspeech_mainloop_handler *h;
    pa_assert(u);
    pa_assert(u->core);
    pa_assert_se(h = pa_msgobject_new(cmtspeech_mainloop_handler));
    h->parent.parent.free = mainloop_handler_free;
    h->parent.process_msg = mainloop_handler_process_msg;
    h->u = u;
    return (pa_msgobject *)h;
}
