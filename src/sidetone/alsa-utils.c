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

#include <asoundlib.h>
#include <pulsecore/macro.h>

/* Retrieve a mixer element by name
 *
 * \param mixer The mixer to search the element from
 * \param element_name The element name to search for */
snd_mixer_elem_t *mixer_get_element(snd_mixer_t *mixer, const char *element_name) {
    pa_assert(mixer);
    pa_assert(element_name);

    snd_mixer_selem_id_t *sid = NULL;
    snd_mixer_elem_t *mixer_element = NULL;

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_name(sid, element_name);
    snd_mixer_selem_id_set_index(sid, 0);

    mixer_element = snd_mixer_find_selem(mixer, sid);

    return mixer_element;
}

/* Get the volume of a mixer element
 *
 * \param mixer The mixer to search the element from.
 * \param element_name The name of the mixer element.
 * \param channel The channel whose volume we want to retrieve.
 * \param playback Whether this is a playback or a capture element.
 * \param[out] volume The retrieved volume.
 * \return 0 on success, otherwise a negative error code.
 */
int mixer_get_element_volume(snd_mixer_t *mixer, const char *element_name, snd_mixer_selem_channel_id_t channel,
                             pa_bool_t playback, long *volume) {
    pa_assert(mixer);
    pa_assert(element_name);
    pa_assert(volume);
    snd_mixer_elem_t *element = NULL;

    element = mixer_get_element(mixer,element_name);

    if(!element) {
        pa_log_error("Unable to open element \"%s\"", element_name);
        return -1;
    }

    if(playback) {
        if(snd_mixer_selem_get_playback_dB(element,channel, volume) < 0) {
            pa_log_error("Unable to get playback volume for element \"%s\"", element_name);
            return -1;
        }
    } else {
        if(snd_mixer_selem_get_capture_dB(element, channel, volume) < 0) {
            pa_log_error("Unable to get capture volume for element \"%s\"", element_name);
            return -1;
        }
    }
   return 0;
}
