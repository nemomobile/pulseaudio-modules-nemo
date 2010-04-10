/*
 * This file is part of pulseaudio-meego
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
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

#ifndef _voice_hooks_h_
#define _voice_hooks_h_

/* TODO: Move to module-voice-api.h and rename in more generic terms */

enum  {
    HOOK_HW_SINK_PROCESS = 0,
    HOOK_NARROWBAND_EAR_EQU_MONO,
    HOOK_NARROWBAND_MIC_EQ_MONO,
    HOOK_WIDEBAND_MIC_EQ_MONO,
    HOOK_XPROT_MONO,
    HOOK_VOLUME,
    HOOK_CALL_VOLUME,
    HOOK_CALL_BEGIN,
    HOOK_CALL_END,
    HOOK_AEP_DOWNLINK,
    HOOK_AEP_UPLINK,
    HOOK_MAX
};

#endif
