/*
 * Copyright (C) 2012 Jolla Ltd.
 * Contact: Tanu Kaskinen <tanu.kaskinen@jollamobile.com>
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
#ifndef _module_stream_restore_nemo_symdef_h
#define _module_stream_restore_nemo_symdef_h

#include <pulsecore/core.h>
#include <pulsecore/macro.h>
#include <pulsecore/module.h>

#define pa__init module_stream_restore_nemo_LTX_pa__init
#define pa__done module_stream_restore_nemo_LTX_pa__done
#define pa__get_author module_stream_restore_nemo_LTX_pa__get_author
#define pa__get_description module_stream_restore_nemo_LTX_pa__get_description
#define pa__get_usage module_stream_restore_nemo_LTX_pa__get_usage
#define pa__get_version module_stream_restore_nemo_LTX_pa__get_version
#define pa__load_once module_stream_restore_nemo_LTX_pa__load_once

int pa__init(struct pa_module*m);
void pa__done(struct pa_module*m);

const char* pa__get_author(void);
const char* pa__get_description(void);
const char* pa__get_usage(void);
const char* pa__get_version(void);
pa_bool_t pa__load_once(void);

#endif
