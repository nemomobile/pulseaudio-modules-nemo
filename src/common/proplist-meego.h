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
#ifndef _PROPLIST_NOKIA_H_
#define _PROPLIST_NOKIA_H_

/* Generic */
#define PA_NOKIA_PROP_AUDIO_MODE                    "x-maemo.mode"                          /* "ihf","hp","hs","hf","a2dp","hsp" */
#define PA_NOKIA_PROP_AUDIO_ACCESSORY_HWID          "x-maemo.accessory_hwid"                /* char */

#define PA_NOKIA_PROP_HOOKS_PTR                     "x-maemo.hooks_ptr"                     /* pa_hook* */

/* Voice module */
#define PA_NOKIA_PROP_AUDIO_CMT_UL_TIMING_ADVANCE   "x-maemo.cmt.ul_timing_advance"         /* int:usecs */
#define PA_NOKIA_PROP_AUDIO_ALT_MIXER_COMPENSATION  "x-maemo.alt_mixer_compensation"        /* int:dB */

#define PA_NOKIA_PROP_AUDIO_AEP_mB_STEPS            "x-maemo.audio_aep_mb_steps"            /* "-6000,-2500, ...*/
#define PA_NOKIA_PROP_AUDIO_SIDETONE_ENABLE         "x-maemo.sidetone.enable"               /* "true" , "false" */
#define PA_NOKIA_PROP_AUDIO_SIDETONE_GAIN_L         "x-maemo.sidetone.lgain"                /* int:*/
#define PA_NOKIA_PROP_AUDIO_SIDETONE_GAIN_R         "x-maemo.sidetone.rgain"                /* int:*/
#define PA_NOKIA_PROP_AUDIO_EAR_REF_PADDING         "x-maemo.ear_ref_padding"               /* int:*/
#define PA_NOKIA_PROP_AUDIO_ACTIVE_MIC_CHANNEL      "x-maemo.active_mic_channel"            /* int:1,2 or 3 (stereo) */

#endif /* _PROPLIST_NOKIA_H_ */
