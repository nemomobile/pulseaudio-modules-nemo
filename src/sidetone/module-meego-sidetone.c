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

#include <pulsecore/module.h>

#include "module-meego-sidetone-symdef.h"

PA_MODULE_AUTHOR("Tanu Kaskinen");
PA_MODULE_DESCRIPTION("Sidetone Controller for ALSA Devices");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(FALSE);
PA_MODULE_USAGE(
        "name=<unique identifier for this sidetone> "
        "output_elements=<list of mixer element names> "
        "input_elements=<list of mixer element names> "
        "control_element=<mixer element name> "
        "initial_target=<db sum as a floating point number> "
        "sinks=<list of sink names> "
        "sources=<list of source names>");

static const char* const valid_modargs[] = {
    "name",
    "output_elements",
    "input_elements",
    "control_elements",
    "target_db_sum",
    "sinks",
    "sources",
    NULL
};

struct userdata {
    pa_module *module;
};

int pa__init(pa_module *m) {
    struct userdata *u = NULL;
    pa_assert(m);

    pa_log("Hello, World!");

    u = m->userdata = pa_xnew0(struct userdata, 1);
    u->module = m;

    return 0;
}

void pa__done(pa_module *m) {
    struct userdata *u = NULL;

    pa_assert(m);

    pa_log("Goodbye, World!");

    if (!(u = m->userdata))
        return;

    pa_xfree(u);
    m->userdata = NULL;
}
