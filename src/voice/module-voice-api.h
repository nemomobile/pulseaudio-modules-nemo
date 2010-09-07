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
#ifndef module_voice_api_h
#define module_voice_api_h

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <pulsecore/sink.h>
#include <pulsecore/source.h>

#define VOICE_SOURCE_FRAMESIZE (20000) /* us */
#define VOICE_SINK_FRAMESIZE (10000) /* us */

#define VOICE_SAMPLE_RATE_HW_HZ   48000
#define VOICE_SAMPLE_RATE_AEP_HZ  8000

#define VOICE_PERIOD_MASTER_USECS 5000
#define VOICE_PERIOD_AEP_USECS    10000
#define VOICE_PERIOD_CMT_USECS    20000

#define VOICE_API_VERSION "0.1"

#define VOICE_HOOK_HW_SINK_PROCESS              "x-meego.voice.hw_sink_process"
#define VOICE_HOOK_NARROWBAND_EAR_EQU_MONO      "x-meego.voice.narrowband_ear_equ_mono"
#define VOICE_HOOK_NARROWBAND_MIC_EQ_MONO       "x-meego.voice.narrowband_mic_eq_mono"
#define VOICE_HOOK_WIDEBAND_MIC_EQ_MONO         "x-meego.voice.wideband_mic_eq_mono"
#define VOICE_HOOK_WIDEBAND_MIC_EQ_STEREO       "x-meego.voice.wideband_mic_eq_stereo"
#define VOICE_HOOK_XPROT_MONO                   "x-meego.voice.xport_mono"
#define VOICE_HOOK_VOLUME                       "x-meego.voice.volume"
#define VOICE_HOOK_CALL_VOLUME                  "x-meego.voice.call_volume"
#define VOICE_HOOK_CALL_BEGIN                   "x-meego.voice.call_begin"
#define VOICE_HOOK_CALL_END                     "x-meego.voice.call_end"
#define VOICE_HOOK_AEP_DOWNLINK                 "x-meego.voice.aep_downlink"
#define VOICE_HOOK_AEP_UPLINK                   "x-meego.voice.aep_uplink"
#define VOICE_HOOK_RMC_MONO                     "x-meego.voice.rmc"


typedef struct {
    pa_memchunk *chunk;
    pa_memchunk *rchunk;
} aep_uplink;

typedef struct {
    pa_memchunk *chunk;
    int spc_flags;
    pa_bool_t cmt;
} aep_downlink;

enum {
    /* TODO: Print out BIG warning if in wrong buffer mode when this message is received */
    VOICE_SOURCE_SET_UL_DEADLINE = PA_SOURCE_MESSAGE_MAX + 100,
};

enum {
    VOICE_SINK_GET_SIDE_INFO_QUEUE_PTR = PA_SINK_MESSAGE_MAX + 100,
};

#define PA_PROP_SINK_API_EXTENSION_PROPERTY_NAME "sink.api-extension.meego.voice"
#define PA_PROP_SINK_API_EXTENSION_PROPERTY_VALUE VOICE_API_VERSION

#define PA_PROP_SOURCE_API_EXTENSION_PROPERTY_NAME "source.api-extension.meego.voice"
#define PA_PROP_SOURCE_API_EXTENSION_PROPERTY_VALUE VOICE_API_VERSION

#define VOICE_MASTER_SINK_INPUT_NAME "Voice module master sink input"
#define VOICE_MASTER_SOURCE_OUTPUT_NAME "Voice module master source output"

/* Because pa_queue does not like NULL pointers, this flag is added to every
 * set of flags to make the pa_queue entries non null. */
#define VOICE_SIDEINFO_FLAG_BOGUS (0x8000)
/* TODO: Create voice API for SPC flags. Voice module should not use cmtspeech headers. */

#endif /* module_voice_api_h */
