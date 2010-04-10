/*
 * This file is part of pulseaudio-meego
 *
 * Copyright (C) 2010 Nokia Corporation. All rights reserved.
 *
 * Contact: Maemo Multimedia <multimedia@maemo.org>
 *
 * This software, including documentation, is protected by copyright
 * controlled by Nokia Corporation. All rights are reserved.
 *
 * Copying, including reproducing, storing, adapting or translating,
 * any or all of this material requires the prior written consent of
 * Nokia Corporation. This material also contains confidential
 * information which may not be disclosed to others without the prior
 * written consent of Nokia.
 */
#ifndef module_music_api_h
#define module_music_api_h

#define MUSIC_API_VERSION "0.1"

#define MUSIC_HOOK_DYNAMIC_ENHANCE              "x-meego.music.dynamic_enhance"
#define MUSIC_HOOK_DYNAMIC_ENHANCE_VOLUME       "x-meego.music.dynamic_enhance_volume"

#define PA_PROP_SINK_MUSIC_API_EXTENSION_PROPERTY_NAME "sink.api-extension.meego.music"
#define PA_PROP_SINK_MUSIC_API_EXTENSION_PROPERTY_VALUE MUSIC_API_VERSION

#endif /* module_music_api_h */
