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

#include "config.h"

#include <string.h>
#include <pulsecore/memblock.h>
#include <pulsecore/memblockq.h>
#include <pulsecore/memchunk.h>

#include "memory.h"

// This should actually be in memblockq.c to check that length % base == 0
int util_memblockq_to_chunk(pa_mempool *mempool, pa_memblockq *memblockq, pa_memchunk *ochunk, size_t length) {

    if (pa_memblockq_get_length(memblockq) >= length) {
        pa_memchunk tchunk = { .memblock = NULL, .length = 0, .index = 0 };
        //pa_assert_se(!pa_memblockq_peek(memblockq, &tchunk));
        if (pa_memblockq_peek(memblockq, &tchunk)) {
            pa_log("pa_memblockq_peek failed unexpectedly (%zu bytes left %zu)", pa_memblockq_get_length(memblockq), tchunk.length);
            return 0;
        }

        if (tchunk.length >= length) { // We can reuse the single block...
            ochunk->memblock = tchunk.memblock;
            ochunk->index = tchunk.index;
            ochunk->length = length;
            pa_memblockq_drop(memblockq, length);
            return 1;
        }
        else {
            char *d, *s; 
            ochunk->memblock = pa_memblock_new(mempool, length);
            ochunk->length = 0;
            ochunk->index = 0;
            d = pa_memblock_acquire(ochunk->memblock);
            while (ochunk->length + tchunk.length <= length) {
                s = pa_memblock_acquire(tchunk.memblock);
                memcpy(d + ochunk->length, s + tchunk.index, tchunk.length);
                ochunk->length += tchunk.length;
                pa_memblock_release(tchunk.memblock);
                pa_memblockq_drop(memblockq, tchunk.length);
                if (tchunk.memblock) {
                    pa_memblock_unref(tchunk.memblock);
                    tchunk.memblock = NULL;
                }
                pa_memblockq_peek(memblockq, &tchunk);
            }
            if (ochunk->length < length) {
                size_t len = length - ochunk->length;
                pa_assert(len < tchunk.length);
                s = pa_memblock_acquire(tchunk.memblock);
                memcpy(d + ochunk->length, s + tchunk.index, len);
                ochunk->length += len;
                pa_memblock_release(tchunk.memblock);
                pa_memblockq_drop(memblockq, len);
            }
            if (tchunk.memblock) {
                pa_memblock_unref(tchunk.memblock);
                tchunk.memblock = NULL;
            }
            pa_memblock_release(ochunk->memblock);
            return 1;
        }
    }
    return 0;
}



