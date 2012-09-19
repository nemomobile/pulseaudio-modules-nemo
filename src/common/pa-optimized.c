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

#include <math.h>

#include "pa-optimized.h"
#include "optimized.h"

int pa_optimized_take_channel(const pa_memchunk *ichunk, pa_memchunk *ochunk, int channel) {
    pa_mempool *pool;
    pa_assert_fp(ochunk);
    pa_assert_fp(ichunk);
    pa_assert_fp(ichunk->memblock);
    pa_assert_fp(channel == 0 || channel == 1);
    pa_assert_fp(0 == (ichunk->length % (16*sizeof(short))));
    pool = pa_memblock_get_pool(ichunk->memblock);

    ochunk->length = ichunk->length/2;
    ochunk->index = 0;
    ochunk->memblock = pa_memblock_new(pool, ochunk->length);
    short *output = (short *) pa_memblock_acquire(ochunk->memblock);
    const short *input = ((short *)pa_memblock_acquire(ichunk->memblock) + ichunk->index/sizeof(short));
    extract_mono_from_interleaved_stereo(input, output, ichunk->length/sizeof(short), channel);
    pa_memblock_release(ochunk->memblock);
    pa_memblock_release(ichunk->memblock);
    return 0;
}

int pa_optimized_downmix_to_mono(const pa_memchunk *ichunk, pa_memchunk *ochunk) {
    pa_mempool *pool;
    pa_assert_fp(ochunk);
    pa_assert_fp(ichunk);
    pa_assert_fp(ichunk->memblock);
    pa_assert_fp(0 == (ichunk->length % (16*sizeof(short))));
    pool = pa_memblock_get_pool(ichunk->memblock);

    ochunk->length = ichunk->length/2;
    ochunk->index = 0;
    ochunk->memblock = pa_memblock_new(pool, ochunk->length);
    short *output = (short *) pa_memblock_acquire(ochunk->memblock);
    const short *input = ((short *)pa_memblock_acquire(ichunk->memblock) + ichunk->index/sizeof(short));
    downmix_to_mono_from_interleaved_stereo(input, output, ichunk->length/sizeof(short));
    pa_memblock_release(ochunk->memblock);
    pa_memblock_release(ichunk->memblock);

    return 0;
}

int pa_optimized_equal_mix_in(pa_memchunk *ochunk, const pa_memchunk *ichunk) {
    pa_assert_fp(ochunk);
    pa_assert_fp(ochunk->memblock);
    pa_assert_fp(ichunk);
    pa_assert_fp(ichunk->memblock);
    pa_assert_fp(ochunk->length == ichunk->length);
    pa_assert_fp(0 == (ichunk->length % (8*sizeof(short))));

    short *output = ((short *)pa_memblock_acquire(ochunk->memblock) + ochunk->index/sizeof(short));
    const short *input = ((short *)pa_memblock_acquire(ichunk->memblock) + ichunk->index/sizeof(short));
    symmetric_mix(input, output, output, ichunk->length/sizeof(short));
    pa_memblock_release(ochunk->memblock);
    pa_memblock_release(ichunk->memblock);

    return 0;
}

int pa_optimized_mix_in_with_volume(pa_memchunk *ochunk, const pa_memchunk *ichunk, const pa_volume_t vol) {
    pa_assert_fp(ochunk);
    pa_assert_fp(ochunk->memblock);
    pa_assert_fp(ichunk);
    pa_assert_fp(ichunk->memblock);
    pa_assert_fp(ochunk->length == ichunk->length);
    pa_assert_fp(0 == (ichunk->length % (8*sizeof(short))));

    short volume = INT16_MAX;
    if (vol < PA_VOLUME_NORM)
	volume = (short) lrint(pa_sw_volume_to_linear(vol)*INT16_MAX);
    pa_log_debug("pavolume 0x%x, volume %d (linear %f)", vol, volume, pa_sw_volume_to_linear(vol));
    short *output = ((short *)pa_memblock_acquire(ochunk->memblock) + ochunk->index/sizeof(short));
    const short *input = ((short *)pa_memblock_acquire(ichunk->memblock) + ichunk->index/sizeof(short));
    mix_in_with_volume(volume, input, output, ichunk->length/sizeof(short));
    pa_memblock_release(ochunk->memblock);
    pa_memblock_release(ichunk->memblock);

    return 0;
}

int pa_optimized_apply_volume(pa_memchunk *chunk, const pa_volume_t vol) {
    pa_assert_fp(chunk);
    pa_assert_fp(chunk->memblock);
    pa_assert_fp(0 == (chunk->length % (8*sizeof(short))));

    short volume = INT16_MAX;
    if (vol < PA_VOLUME_NORM)
	volume = (short) lrint(pa_sw_volume_to_linear(vol)*INT16_MAX);
    short *input = ((short *)pa_memblock_acquire(chunk->memblock) + chunk->index/sizeof(short));
    apply_volume(volume, input, input, chunk->length/sizeof(short));
    pa_memblock_release(chunk->memblock);

    return 0;
}

int pa_optimized_mono_to_stereo(const pa_memchunk *ichunk, pa_memchunk *ochunk) {
    pa_mempool *pool;
    pa_assert_fp(ochunk);
    pa_assert_fp(ichunk);
    pa_assert_fp(ichunk->memblock);
    pa_assert_fp(0 == (ichunk->length % (8*sizeof(short))));
    pool = pa_memblock_get_pool(ichunk->memblock);

    ochunk->length = 2*ichunk->length;
    ochunk->index = 0;
    ochunk->memblock = pa_memblock_new(pool, ochunk->length);
    short *output = (short *) pa_memblock_acquire(ochunk->memblock);
    const short *input = ((short *)pa_memblock_acquire(ichunk->memblock) + ichunk->index/sizeof(short));
    dup_mono_to_interleaved_stereo(input, output, ichunk->length/sizeof(short));
    pa_memblock_release(ochunk->memblock);
    pa_memblock_release(ichunk->memblock);

    return 0;
}

int pa_optimized_interleave_stereo(const pa_memchunk *ichunk1, const pa_memchunk *ichunk2, pa_memchunk *ochunk) {
    pa_mempool *pool;
    pa_assert_fp(ochunk);
    pa_assert_fp(ichunk1);
    pa_assert_fp(ichunk2);
    pa_assert_fp(ichunk1->memblock);
    pa_assert_fp(ichunk2->memblock);
    pa_assert_fp(0 == (ichunk1->length % (8*sizeof(short))));
    pa_assert_fp(ichunk1->length == ichunk2->length);
    pool = pa_memblock_get_pool(ichunk1->memblock);

    ochunk->length = 2*ichunk1->length;
    ochunk->index = 0;
    ochunk->memblock = pa_memblock_new(pool, ochunk->length);
    short *output = (short *) pa_memblock_acquire(ochunk->memblock);
    const short *input1 = ((short *)pa_memblock_acquire(ichunk1->memblock) + ichunk1->index/sizeof(short));
    const short *input2 = ((short *)pa_memblock_acquire(ichunk2->memblock) + ichunk2->index/sizeof(short));
    const short *bufs[2] = { input1, input2 };
    interleave_mono_to_stereo(bufs, output, ichunk1->length/sizeof(short));
    pa_memblock_release(ochunk->memblock);
    pa_memblock_release(ichunk1->memblock);
    pa_memblock_release(ichunk2->memblock);

    return 0;
}

int pa_optimized_deinterleave_stereo_to_mono(const pa_memchunk *ichunk, pa_memchunk *ochunk1, pa_memchunk *ochunk2) {
    pa_mempool *pool;
    pa_assert_fp(ichunk);
    pa_assert_fp(ochunk1);
    pa_assert_fp(ochunk2);
    pa_assert_fp(ichunk->memblock);
    pa_assert_fp(0 == (ichunk->length % (8*sizeof(short))));
    pool = pa_memblock_get_pool(ichunk->memblock);

    ochunk1->length = ichunk->length/2;
    ochunk1->index = 0;

    ochunk2->length = ichunk->length/2;
    ochunk2->index = 0;

    ochunk1->memblock = pa_memblock_new(pool, ochunk1->length);
    ochunk2->memblock = pa_memblock_new(pool, ochunk2->length);

    short *output1 = (short *) pa_memblock_acquire(ochunk1->memblock);
    short *output2 = (short *) pa_memblock_acquire(ochunk2->memblock);

    /* set input pointer to correct position */
    const short *input = ((short *)pa_memblock_acquire(ichunk->memblock) + ichunk->index/sizeof(short));

    short *bufs[2] = { output1, output2 };
    deinterleave_stereo_to_mono(input, bufs, ichunk->length/sizeof(short));

    pa_memblock_release(ichunk->memblock);
    pa_memblock_release(ochunk1->memblock);
    pa_memblock_release(ochunk2->memblock);

    return 0;
}
