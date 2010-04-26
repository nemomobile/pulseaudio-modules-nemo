/***
  This file is part of pulseaudio-meego.

  Copyright (C) 2010 Nokia Corporation. All rights reserved.

  Contact: Maemo Multimedia <multimedia@maemo.org>

  This software, including documentation, is protected by copyright
  controlled by Nokia Corporation. All rights are reserved.

  Copying, including reproducing, storing, adapting or translating,
  any or all of this material requires the prior written consent of
  Nokia Corporation. This material also contains confidential
  information which may not be disclosed to others without the prior
  written consent of Nokia.
***/

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
