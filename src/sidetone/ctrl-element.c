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

#include <pulsecore/macro.h>
#include <pulse/xmalloc.h>

#include <asoundlib.h>

#include "alsa-utils.h"
#include "ctrl-element.h"

struct ctrl_element {
    snd_mixer_t *mixer;

    /* We'll store the element name and query the element pointer from the
     * mixer every time we set the element volume. This is how it's done in
     * PulseAudio, I guess in case the element "disappears", so let's just
     * follow the same convention. */
    const char *element_name;

    long min_dB;
    long max_dB;
};

ctrl_element *ctrl_element_new(snd_mixer_t *mixer, const char* name) {
    pa_assert(mixer);
    pa_assert(name);

    ctrl_element *ctrl = NULL;
    snd_mixer_elem_t *element = NULL;
    long volume = 0;

    ctrl = pa_xnew0(ctrl_element, 1);
    ctrl->mixer = mixer;
    ctrl->element_name = pa_xstrdup(name);

    element = mixer_get_element(mixer, name);
    if(!element) {
        pa_log_error("Unable to open mixer element \"%s\"", name);
        goto fail;
    }

    if(!snd_mixer_selem_is_playback_mono(element) ||
       !snd_mixer_selem_has_playback_volume(element)) {
        pa_log_error("\"%s\", is not a valid sidetone control element", name);
        goto fail;
    }

    if(snd_mixer_selem_get_playback_dB_range(element,  &ctrl->min_dB, &ctrl->max_dB) < 0) {
        pa_log_error("Failed to get playback volume range for sidetone control element");
        goto fail;
    }

    if(snd_mixer_selem_get_playback_volume(element, SND_MIXER_SCHN_MONO, &volume) < 0) {
        pa_log_error("Failed to get playback volume for sidetone control element");
        goto fail;
    }

    pa_log_debug("Sidetone control element initialized for \"%s\", Volume range (%ld .. %ld)", 
            name, ctrl->min_dB, ctrl->max_dB);

    return ctrl;

fail:

    pa_xfree(ctrl);

    return NULL;
}

void ctrl_element_free(ctrl_element *ctrl) {
    pa_assert(ctrl);
    pa_xfree(ctrl->element_name);
    pa_xfree(ctrl);
}

int ctrl_element_set_volume(ctrl_element *ctrl, long volume) {
    pa_assert(ctrl);

    snd_mixer_elem_t *element = NULL;
    long actual_volume = 0;

    pa_log_debug("Setting sidetone control element volume to %ld", volume);

    element = mixer_get_element(ctrl->mixer, ctrl->element_name);
    if(!element) {
        pa_log_error("Element %s has disappeared.", ctrl->element_name);
        return -1;
    }

    if(volume > ctrl->max_dB) {
        pa_log_warn("Required sidetone compensation volume is greater than %ld", ctrl->max_dB);
        volume = ctrl->max_dB;
    } else if(volume < ctrl->min_dB) {
        pa_log_warn("Required sidetone compensation volume is smaller than %ld", ctrl->min_dB);
        volume = ctrl->min_dB;
    }

    /* Set the volume accurately or round up. */
    if((snd_mixer_selem_set_playback_dB(element, SND_MIXER_SCHN_MONO, volume, 1) < 0)) { /* 1 = Accurate or 1st above */
        pa_log_error("Failed to set volume for control element");
        return -1;
    }

    if(snd_mixer_selem_get_playback_dB(element, SND_MIXER_SCHN_MONO, &actual_volume) < 0) {
        pa_log_error("Failed to get actual element volume");
        return -1;
    }

    pa_log_debug("Control element volume is now %ld (should be close to %ld)", actual_volume, volume);

    return 0;
}

int ctrl_element_mute(ctrl_element *ctrl) {
    pa_assert(ctrl);

    snd_mixer_elem_t *element = NULL;

    element = mixer_get_element(ctrl->mixer, ctrl->element_name);
    if(!element) {
        pa_log_error("Element %s has disappeared.", ctrl->element_name);
        return -1;
    }

    if((snd_mixer_selem_set_playback_volume(element, SND_MIXER_SCHN_MONO, 0) < 0)) {
        pa_log_error("Failed to mute sidetone element");
        return -1;
    }

    return 0;
}

