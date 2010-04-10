/*
 * This file is part of pulseaudio-meego
 *
 * Copyright (C) 2008, 2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Maemo Multimedia <multimedia@maemo.org>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved.
 *
 * Copying, including reproducing, storing, adapting or translating,
 * any or all of this material requires the prior written consent of
 * Nokia Corporation. This material also contains confidential
 * information which may not be disclosed to others without the prior
 * written consent of Nokia.
 */
#ifndef module_voice_userdata_h
#define module_voice_userdata_h

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include <pulsecore/modargs.h>
#include <pulsecore/sink-input.h>
#include <pulsecore/source-output.h>
#include <pulsecore/module.h>
#include <pulsecore/thread.h>
#include <pulsecore/thread-mq.h>
#include <pulsecore/semaphore.h>
#include <pulsecore/fdsem.h>
#include <pulsecore/call-state-tracker.h>

#ifdef HAVE_LIBCMTSPEECHDATA
#include <cmtspeech.h>
#endif

#include "src/common/src-48-to-8.h"
#include "src/common/src-8-to-48.h"

#include "algorithm-hook.h"

#include <voice-buffer.h>
#include <voice-hooks.h>

/* This is a copy/paste from module-alsa-sink-volume.c, keep it up to date!*/
/* String with single integer defining which mixer
 * tuning table is used. Currently only two different tables
 * can be defined.
 */
#define PROP_MIXER_TUNING_MODE "x-maemo.alsa_sink.mixer_tuning_mode"
#define PROP_MIXER_TUNING_PRI (0)
#define PROP_MIXER_TUNING_ALT (1)

#define PROP_MIXER_TUNING_PRI_S "0"
#define PROP_MIXER_TUNING_ALT_S "1"

/* TODO: Classify each member according to which thread they are used from */
struct userdata {
    pa_core *core;
    pa_module *module;
    pa_modargs *modargs;

    algorithm_hook *algorithm;

    pa_msgobject *mainloop_handler;

    int ul_timing_advance; /* usecs */

    pa_channel_map mono_map;
    pa_channel_map stereo_map;
    pa_sample_spec hw_sample_spec;
    pa_sample_spec hw_mono_sample_spec;

    pa_sample_spec aep_sample_spec;
    pa_channel_map aep_channel_map;
    size_t aep_fragment_size;

    size_t aep_hw_fragment_size;
    size_t hw_fragment_size;
    size_t hw_fragment_size_max;
    size_t hw_mono_fragment_size;
    size_t aep_hw_mono_fragment_size;

    size_t voice_ul_fragment_size;

    pa_memchunk aep_silence_memchunk;

    pa_atomic_ptr_t memchunk_pool;

    pa_sink *master_sink;
    pa_source *master_source;

    pa_sink *raw_sink;
    pa_sink *voip_sink;

    pa_sink_input *hw_sink_input;
    pa_hook_slot *hw_sink_input_move_fail_slot;
    pa_hook_slot *hw_sink_input_move_finish_slot;
    pa_atomic_t mixer_state;
    pa_volume_t alt_mixer_compensation;
    void *sink_temp_buff;
    size_t sink_temp_buff_len;

    pa_sink_input *aep_sink_input;

    pa_source *raw_source;
    pa_source *voip_source;

    pa_source_output *hw_source_output;
    pa_hook_slot *hw_source_output_move_fail_slot;

    pa_memblockq *hw_source_memblockq;

    pa_memblockq *ul_memblockq;

    int64_t ul_deadline;   /* Comparabel to pa_rtclock_now() */

    int16_t linear_q15_master_volume_L;
    int16_t linear_q15_master_volume_R;

    pa_queue *dl_sideinfo_queue;

    src_48_to_8 *hw_source_to_aep_resampler;
    src_8_to_48 *aep_to_hw_sink_resampler;
    src_48_to_8 *ear_to_aep_resampler;
    src_48_to_8 *raw_sink_to_hw8khz_sink_resampler;
    src_8_to_48 *hw8khz_source_to_raw_source_resampler;

    struct voice_aep_ear_ref {
	int loop_padding_usec;
	pa_atomic_t loop_state;
	volatile struct timeval loop_tstamp;
	pa_asyncq *loop_asyncq;
	pa_memblockq *loop_memblockq;
    } ear_ref;

    pa_hook_slot *sink_proplist_changed_slot;
    pa_hook_slot *source_proplist_changed_slot;

    pa_subscription *sink_subscription;

    pa_call_state_tracker *call_state_tracker;

    unsigned current_audio_mode_hwid_hash;
    pa_hook *hooks[HOOK_MAX];
};


#endif /* module_voice_userdata_h */
