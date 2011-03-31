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

#include <pulsecore/namereg.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/source-output.h>
#include <pulsecore/log.h>
#include <pulsecore/mutex.h>
#include <pulsecore/thread.h>
#include <pulsecore/sample-util.h>

#include "module-meego-cmtspeech.h"
#include "cmtspeech-sink-input.h"
#include "cmtspeech-connection.h"
#include "memory.h"
#include "module-voice-api.h"

/**
 * Converts speech frame sideinfo flags from libcmtspeechdata format
 * to that of pulseaudio-meego voice module.
 */
static unsigned int cmtspeech_to_voice_spc_flags(unsigned int spc_flags)
{
    switch(spc_flags)
        {
        case CMTSPEECH_SPC_FLAGS_SPEECH: return VOICE_SIDEINFO_FLAG_SPEECH;
        case CMTSPEECH_SPC_FLAGS_BFI: return VOICE_SIDEINFO_FLAG_BAD;
        default:
            ;
        }
    return 0;
}

static void cmtspeech_dl_sideinfo_push(unsigned int cmt_spc_flags, int length, struct userdata *u) {
    unsigned int spc_flags;
    pa_assert(length % u->dl_frame_size == 0);
    pa_assert(u);

    if (NULL == u->voice_sideinfoq)
        return;

    spc_flags = cmtspeech_to_voice_spc_flags(cmt_spc_flags);
    spc_flags |= VOICE_SIDEINFO_FLAG_BOGUS;

    while (length) {
        pa_queue_push(u->local_sideinfoq, PA_UINT_TO_PTR(spc_flags));
        length -= u->dl_frame_size;
    }
}

static void cmtspeech_dl_sideinfo_drop(struct userdata *u, int length) {

    pa_assert(u);
    pa_assert(length % u->dl_frame_size == 0);

    if (NULL == u->voice_sideinfoq)
        return;

    while (length) {
        pa_queue_pop(u->local_sideinfoq);
        length -= u->dl_frame_size;
    }

    u->continuous_dl_stream = FALSE;
}

static void cmtspeech_dl_sideinfo_forward(struct userdata *u) {
    unsigned int spc_flags = 0;

    pa_assert(u);

    if (NULL == u->voice_sideinfoq)
        return;

    spc_flags = PA_PTR_TO_UINT(pa_queue_pop(u->local_sideinfoq));

    if (spc_flags == 0) {
        pa_log_warn("Local sideinfo queue empty.");
        spc_flags = VOICE_SIDEINFO_FLAG_BAD|VOICE_SIDEINFO_FLAG_BOGUS;
    }
    else if (!u->continuous_dl_stream)
        spc_flags |= VOICE_SIDEINFO_FLAG_BAD;

    u->continuous_dl_stream = TRUE;

    pa_queue_push(u->voice_sideinfoq, PA_UINT_TO_PTR(spc_flags));
}

static void cmtspeech_dl_sideinfo_bogus(struct userdata *u) {
    unsigned int spc_flags = VOICE_SIDEINFO_FLAG_BAD|VOICE_SIDEINFO_FLAG_BOGUS;

    pa_assert(u);

    if (NULL == u->voice_sideinfoq)
        return;

    pa_queue_push(u->voice_sideinfoq, PA_UINT_TO_PTR(spc_flags));

    u->continuous_dl_stream = FALSE;
}

static void cmtspeech_dl_sideinfo_flush(struct userdata *u) {
    pa_assert(u);
    ENTER();

    while (pa_queue_pop(u->local_sideinfoq))
        ;

    if (u->voice_sideinfoq) {
        while (pa_queue_pop(u->voice_sideinfoq))
            ;
    }
}

/*** sink_input callbacks ***/
static int cmtspeech_sink_input_pop_cb(pa_sink_input *i, size_t length, pa_memchunk *chunk) {
    struct userdata *u;
    int queue_counter = 0;

    pa_assert_fp(i);
    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);
    pa_assert_fp(chunk);

    if (u->cmt_connection.dl_frame_queue) {
        cmtspeech_dl_buf_t *buf;
        while ((buf = pa_asyncq_pop(u->cmt_connection.dl_frame_queue, FALSE))) {
            pa_memchunk cmtchunk;
            if (cmtspeech_buffer_to_memchunk(u, buf, &cmtchunk) < 0)
                continue;
            queue_counter++;
            if (pa_memblockq_push(u->dl_memblockq, &cmtchunk) < 0) {
                pa_log_debug("Failed to push DL frame to dl_memblockq (len %d max %d )",
                             pa_memblockq_get_length(u->dl_memblockq),
                             pa_memblockq_get_maxlength(u->dl_memblockq));
            }
            else {
                cmtspeech_dl_sideinfo_push(buf->spc_flags, cmtchunk.length, u);
            }
            pa_memblock_unref(cmtchunk.memblock);
        }
    }

    /* More than one DL frame in queue means that sink has not asked for more
     * data for over 20ms and something may be wrong. */
    if (queue_counter > 1) {
        pa_log_info("%d frames found from queue (dl buf size %d)", queue_counter,
                    pa_memblockq_get_length(u->dl_memblockq));
    }

    if (pa_memblockq_get_length(u->dl_memblockq) > 3*u->dl_frame_size) {
        size_t drop_bytes =
            pa_memblockq_get_length(u->dl_memblockq) - 3*u->dl_frame_size;
        pa_memblockq_drop(u->dl_memblockq, drop_bytes);
        cmtspeech_dl_sideinfo_drop(u, drop_bytes);
        pa_log_debug("Too much data in DL buffer dropped %d bytes",
                     drop_bytes);
    }

    pa_assert_fp((pa_memblockq_get_length(u->dl_memblockq) % u->dl_frame_size) == 0);

    if (util_memblockq_to_chunk(u->core->mempool, u->dl_memblockq, chunk, u->dl_frame_size)) {
        ONDEBUG_TOKENS(fprintf(stderr, "d"));
        cmtspeech_dl_sideinfo_forward(u);
    }
    else {
        if (u->cmt_connection.first_dl_frame_received && pa_log_ratelimit())
            pa_log_debug("No DL audio: %d bytes in queue %d needed",
                         pa_memblockq_get_length(u->dl_memblockq), u->dl_frame_size);
        cmtspeech_dl_sideinfo_bogus(u);
        pa_silence_memchunk_get(&u->core->silence_cache,
                                u->core->mempool,
                                chunk,
                                &u->ss,
                                u->dl_frame_size);
    }

    return 0;
}

/* Called from I/O thread context */
static void cmtspeech_sink_input_process_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (!PA_SINK_INPUT_IS_LINKED(i->thread_info.state))
        return;

    pa_log_debug("%s rewound %u bytes", i->sink->name, nbytes);
}

/* Called from I/O thread context */
static void cmtspeech_sink_input_update_max_rewind_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (!PA_SINK_INPUT_IS_LINKED(i->thread_info.state))
        return;

    pa_log_debug("Max rewind of %s updated to %u bytes", i->sink->name, nbytes);
}

/* Called from I/O thread context */
static void cmtspeech_sink_input_update_max_request_cb(pa_sink_input *i, size_t nbytes) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (!PA_SINK_INPUT_IS_LINKED(i->thread_info.state))
        return;

    pa_log_debug("Max request of %s updated to %u bytes", i->sink->name, nbytes);
}

/* Called from I/O thread context */
static void cmtspeech_sink_input_update_sink_latency_range_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_log_debug("Latency range changed to %lld - %lld usec",
                 i->sink->thread_info.min_latency, i->sink->thread_info.max_latency);
}

/* Called from I/O thread context */
static void cmtspeech_sink_input_reset_dl_stream(struct userdata *u) {
    cmtspeech_dl_buf_t *buf;
    pa_assert(u);

    /* Flush all DL buffers */
    pa_memblockq_flush_read(u->dl_memblockq);
    cmtspeech_dl_sideinfo_flush(u);
    while ((buf = pa_asyncq_pop(u->cmt_connection.dl_frame_queue, FALSE))) {
        pa_memchunk cmtchunk;
        cmtspeech_buffer_to_memchunk(u, buf, &cmtchunk);
        pa_memblock_unref(cmtchunk.memblock);
    }
}

/* Called from I/O thread context */
static void cmtspeech_sink_input_detach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    u->sink = NULL;
    u->voice_sideinfoq = NULL;

    cmtspeech_sink_input_reset_dl_stream(u);

    pa_log_debug("Detach called for CMT sink input");
}

/* Called from I/O thread context */
static void cmtspeech_sink_input_attach_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    u->voice_sideinfoq = NULL;

    PA_MSGOBJECT(i->sink)->process_msg(
        PA_MSGOBJECT(i->sink), VOICE_SINK_GET_SIDE_INFO_QUEUE_PTR, &u->voice_sideinfoq, (int64_t)0, NULL);

    pa_log_debug("CMT sink input connected to %s (side info queue = %p)", i->sink->name,
                 (void*) u->voice_sideinfoq);

    cmtspeech_dl_sideinfo_flush(u);
}

/* Called from I/O thread context */
static void cmtspeech_sink_input_state_change_cb(pa_sink_input *i, pa_sink_input_state_t state) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    pa_log_debug("State changed %d -> %d", i->thread_info.state, state);
}

/* Called from I/O thread context */
static int cmtspeech_sink_input_process_msg(pa_msgobject *o, int code, void *userdata, int64_t offset, pa_memchunk *chunk) {
    struct userdata *u;
    pa_sink_input *i = PA_SINK_INPUT(o);
    pa_sink_input_assert_ref(i);

    pa_assert_se(u = i->userdata);

    switch (code) {
        case PA_SINK_INPUT_MESSAGE_FLUSH_DL:
            cmtspeech_sink_input_reset_dl_stream(u);
            pa_log_info("PA_SINK_INPUT_MESSAGE_FLUSH_DL handled");
            return 0;
    }

    return pa_sink_input_process_msg(o, code, userdata, offset, chunk);
}

/* Called from main context */
static void cmtspeech_sink_input_kill_cb(pa_sink_input *i) {
    struct userdata *u;

    pa_log_debug("Kill called");

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);
    pa_assert(u->sink_input == i);

    pa_log_warn("Kill called for cmtspeech sink input");
    cmtspeech_trigger_unload(u);

    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;
}

/* These callbacks do not have any relevance as long as we apply
   PA_SINK_INPUT_DONT_MOVE flag */
/* Called from main context */
static void cmtspeech_sink_input_moving_cb(pa_sink_input *i, pa_sink *dest){
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    u->sink = i->sink;

    pa_log_debug("CMT Sink input moving to %s", dest ? dest->name : "(null)");
}

/* Called from main context */
static pa_bool_t cmtspeech_sink_input_may_move_to_cb(pa_sink_input *i, pa_sink *dest) {
    struct userdata *u;

    pa_sink_input_assert_ref(i);
    pa_assert_se(u = i->userdata);

    if (cmtspeech_check_sink_api(dest))
        return FALSE;

    return TRUE;
}

int cmtspeech_create_sink_input(struct userdata *u) {
    pa_sink_input_new_data data;
    char t[256];

    pa_assert(u);
    pa_assert(!u->sink);
    ENTER();

    if (u->sink_input) {
        pa_log_warn("Create called but input already exists");
        return 1;
    }

    if (!(u->sink = pa_namereg_get(u->core, u->sink_name, PA_NAMEREG_SINK))) {
        pa_log_error("Couldn't find sink %s", u->sink_name);
        return 2;
    }

    if (cmtspeech_check_sink_api(u->sink))
        return 3;

    pa_sink_input_new_data_init(&data);
    data.driver = __FILE__;
    data.module = u->module;
    data.sink = u->sink;
    snprintf(t, sizeof(t), "Cellular call down link");
    pa_proplist_sets(data.proplist, PA_PROP_MEDIA_NAME, t);
    snprintf(t, sizeof(t), "phone");
    pa_proplist_sets(data.proplist, PA_PROP_MEDIA_ROLE, t);
    snprintf(t, sizeof(t), "cmtspeech module");
    pa_proplist_sets(data.proplist, PA_PROP_APPLICATION_NAME, t);
    pa_sink_input_new_data_set_sample_spec(&data, &u->ss);
    pa_sink_input_new_data_set_channel_map(&data, &u->map);
    data.flags = PA_SINK_INPUT_DONT_MOVE|PA_SINK_INPUT_START_CORKED;

    pa_sink_input_new(&u->sink_input, u->core, &data);
    pa_sink_input_new_data_done(&data);

    if (!u->sink_input) {
        pa_log_warn("Creating cmtspeech sink input failed");
        return -1;
    }

    u->sink_input->parent.process_msg = cmtspeech_sink_input_process_msg;
    u->sink_input->pop = cmtspeech_sink_input_pop_cb;
    u->sink_input->process_rewind = cmtspeech_sink_input_process_rewind_cb;
    u->sink_input->update_max_rewind = cmtspeech_sink_input_update_max_rewind_cb;
    u->sink_input->update_max_request = cmtspeech_sink_input_update_max_request_cb;
    u->sink_input->update_sink_latency_range = cmtspeech_sink_input_update_sink_latency_range_cb;
    u->sink_input->kill = cmtspeech_sink_input_kill_cb;
    u->sink_input->attach = cmtspeech_sink_input_attach_cb;
    u->sink_input->detach = cmtspeech_sink_input_detach_cb;
    u->sink_input->moving = cmtspeech_sink_input_moving_cb;
    u->sink_input->state_change = cmtspeech_sink_input_state_change_cb;
    u->sink_input->may_move_to = cmtspeech_sink_input_may_move_to_cb;
    u->sink_input->userdata = u;

    pa_sink_input_put(u->sink_input);

    pa_log_info("cmtspeech sink-input created");

    return 0;
}

void cmtspeech_delete_sink_input(struct userdata *u) {
    pa_assert(u);
    ENTER();

    if (!u->sink_input) {
        pa_log_info("Delete called but no sink input exists");
        return;
    }

    pa_sink_input_unlink(u->sink_input);
    pa_sink_input_unref(u->sink_input);
    u->sink_input = NULL;

    pa_log_info("cmtspeech sink-input deleted");
}
