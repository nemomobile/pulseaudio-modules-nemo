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

#ifndef _voice_hooks_h_
#define _voice_hooks_h_

/* TODO: Move to module-voice-api.h and rename in more generic terms */

enum  {
    HOOK_HW_SINK_PROCESS = 0,
    HOOK_NARROWBAND_EAR_EQU_MONO,
    HOOK_NARROWBAND_MIC_EQ_MONO,
    HOOK_WIDEBAND_MIC_EQ_MONO,
    HOOK_WIDEBAND_MIC_EQ_STEREO,
    HOOK_XPROT_MONO,
    HOOK_VOLUME,
    HOOK_CALL_VOLUME,
    HOOK_CALL_BEGIN,
    HOOK_CALL_END,
    HOOK_AEP_DOWNLINK,
    HOOK_AEP_UPLINK,
    HOOK_RMC_MONO,
    HOOK_SOURCE_RESET,
    HOOK_MAX
};

#endif
