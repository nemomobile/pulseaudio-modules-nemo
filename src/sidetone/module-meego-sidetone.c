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
#include <src/common/parameter-hook.h>

#include "module-meego-sidetone-symdef.h"
#include "sidetone.h"
#include "sidetone-args.h"

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
        "sources=<list of source names>"
        "sink_path=<sink path we are interested in>");

struct userdata {
    pa_module *module;
    sidetone *sidetone;
    char* argument;
    pa_hook_slot  *sidetone_parameters_updates;
};



static pa_hook_result_t parameters_changed_cb(pa_core *c, struct update_args *ua, struct userdata *u) {

    sidetone *st;
    st=u->sidetone;
    pa_assert(u);

    if( ua ){
        if(ua->parameters){
           if(NULL != u->argument)
                pa_xfree(u->argument);
            u->argument=pa_xmalloc0((ua->length+1)*sizeof(char));
            pa_assert(u->argument);
            strcpy(u->argument,ua->parameters);
            *(u->argument+ua->length+1)='\0';
        }

        if(st){
            sidetone_free(u->sidetone);
            u->sidetone = NULL;
        }

        pa_log_debug("Sidetone parameters updates with %s",(char*)u->argument);

        if(!(u->sidetone = sidetone_new(u->module->core, u->argument))) {
            pa_xfree(u->argument);
            u->argument=NULL;
            return -1;
        }
    }
    else {
        if(st){
            sidetone_free(u->sidetone);
            u->sidetone = NULL;
        }
    }

    return PA_HOOK_OK;
}

int pa__init(pa_module *m) {
    pa_assert(m);

    struct userdata *u = NULL;

    u = m->userdata = pa_xnew0(struct userdata, 1);
    u->module=m;
    u->argument=NULL;
    u->sidetone=NULL;
    u->sidetone_parameters_updates = request_parameter_updates("alsa-sidetone", (pa_hook_cb_t)parameters_changed_cb, PA_HOOK_NORMAL, u);
    return 0;
}

void pa__done(pa_module *m) {
    pa_assert(m);
    pa_assert(m->userdata);

    struct userdata *u = m->userdata;

    if(u) {
        pa_xfree(u->argument);
        sidetone_free(u->sidetone);
        pa_xfree(u);
    }

    m->userdata = NULL;
}
