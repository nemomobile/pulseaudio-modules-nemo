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
#include "sidetone.h"

PA_MODULE_AUTHOR("Tanu Kaskinen & Antti-Ville Jansson");
PA_MODULE_DESCRIPTION("Sidetone Controller for ALSA Devices");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(FALSE);
PA_MODULE_USAGE(
        "mixer=<name of the ALSA mixer> "
        "input_elements=<list of mixer element names> "
        "output_elements=<list of mixer element names> "
        "control_element=<mixer element name> "
        "target_volume=<initial target volume as an integer (dB * 100)> "
        "sinks=<list of sink names> "
        "sources=<list of source names>");

struct userdata {
    pa_module *module;
    sidetone *sidetone;
};

int pa__init(pa_module *m) {
    pa_assert(m);

    struct userdata *u = NULL;

    u = m->userdata = pa_xnew0(struct userdata, 1);
    u->module = m;

    if(!(u->sidetone = sidetone_new(m->core, m->argument))) {
        pa_xfree(u);
        m->userdata = NULL;
        return -1;
    }

    return 0;
}

void pa__done(pa_module *m) {
    pa_assert(m);
    pa_assert(m->userdata);

    struct userdata *u = m->userdata;

    if(u) {
        sidetone_free(u->sidetone);
        pa_xfree(u);
    }

    m->userdata = NULL;
}

