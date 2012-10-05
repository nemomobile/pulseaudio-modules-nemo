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

#ifndef cmtspeech_connection_h
#define cmtspeech_connection_h

#include "module-meego-cmtspeech.h"
#include "cmtspeech-mainloop-handler.h"

#define CMTSPEECH_DBUS_CSCALL_CONNECT_IF    "com.nokia.csd.Call.Instance"
#define CMTSPEECH_DBUS_CSCALL_CONNECT_SIG   "AudioConnect"

#define CMTSPEECH_DBUS_CSCALL_STATUS_IF     "com.nokia.csd.Call"
#define CMTSPEECH_DBUS_CSCALL_STATUS_SIG    "ServerStatus"

#define CMTSPEECH_DBUS_PHONE_SSC_STATE_IF   "com.nokia.phone.SSC"
#define CMTSPEECH_DBUS_PHONE_SSC_STATE_SIG  "modem_state_changed_ind"

#define OFONO_DBUS_VOICECALL_IF         "org.ofono.VoiceCall"
#define OFONO_DBUS_VOICECALL_CHANGE_SIG "PropertyChanged"
#define ALSA_OLD_ALTERNATIVE_PROP       "x-maemo.alsa_sink.buffers=alternative"
#define ALSA_OLD_PRIMARY_PROP           "x-maemo.alsa_sink.buffers=primary"
#define OFONO_DBUS_VOICECALL_ACTIVE       "active"
#define OFONO_DBUS_VOICECALL_HELD         "held"
#define OFONO_DBUS_VOICECALL_DIALING      "dialing"
#define OFONO_DBUS_VOICECALL_ALERTING     "alerting"
#define OFONO_DBUS_VOICECALL_INCOMING     "incoming"
#define OFONO_DBUS_VOICECALL_WAITING      "waiting"
#define OFONO_DBUS_VOICECALL_DISCONNECTED "disconnected"

#include <cmtspeech_msgs.h>

typedef cmtspeech_buffer_t cmtspeech_dl_buf_t;

int cmtspeech_connection_init(struct userdata *u);
void cmtspeech_connection_unload(struct userdata *u);

int cmtspeech_send_ul_frame(struct userdata *u, uint8_t *buf, size_t bytes);

int cmtspeech_buffer_to_memchunk(struct userdata *u, cmtspeech_dl_buf_t *buf, pa_memchunk *chunk);

DBusHandlerResult cmtspeech_dbus_filter(DBusConnection *conn, DBusMessage *msg, void *arg);

#endif /* cmtspeech_connection_h */
