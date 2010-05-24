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

#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <pulse/xmalloc.h>
#include <pulse/proplist.h>

#include <pulsecore/module.h>
#include <pulsecore/modargs.h>
#include <pulsecore/namereg.h>
#include <pulsecore/log.h>

#include "src/common/proplist-meego.h"

#include "module-meego-test-symdef.h"

PA_MODULE_AUTHOR("Pekka Ervasti");
PA_MODULE_DESCRIPTION("test module");
PA_MODULE_USAGE("sink_name=<name of hw sink> audio_mode=<ihf,hs> hwid=<accessory hwid>");
PA_MODULE_VERSION(PACKAGE_VERSION);

static const char* const valid_modargs[] = {
    "sink_name",
    "audio_mode",
    "hwid",
    NULL,
};

struct userdata {
    pa_core *core;
    pa_module *module;
};

int pa__init(pa_module*m) {
    pa_modargs *ma = NULL;
    struct userdata *u;
    const char *sink_name;
    const char *audio_mode;
    const char *hwid;
    pa_sink *hw_sink;
    pa_proplist *p;

    u = pa_xmalloc(sizeof(struct userdata));

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u;
    u->core = m->core;
    u->module = m;

    sink_name = pa_modargs_get_value(ma, "sink_name", NULL);
    hw_sink = pa_namereg_get(m->core, sink_name, PA_NAMEREG_SINK);

    if (!hw_sink) {
        pa_log("No such sink: %s", sink_name);
        goto fail;
    }

    audio_mode = pa_modargs_get_value(ma, "audio_mode", "ihf");
    hwid = pa_modargs_get_value(ma, "hwid", "");

    p = pa_proplist_new();

    pa_proplist_sets(p, PA_NOKIA_PROP_AUDIO_MODE, audio_mode);
    pa_proplist_sets(p, PA_NOKIA_PROP_AUDIO_ACCESSORY_HWID, hwid);

    pa_sink_update_proplist(hw_sink, PA_UPDATE_REPLACE, p);
    pa_proplist_free(p);

    pa_modargs_free(ma);

    /* Work done, let's unload */
    pa_module_unload_request(m, TRUE);

    return 0;

 fail:
    if (ma)
        pa_modargs_free(ma);

    pa_xfree(u);
    m->userdata = NULL;

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata *u = m->userdata;
    assert(m);

    if (!u)
        return;

    pa_xfree(u);
}
