/*
 * Copyright (c) 2024, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <string.h>
#include <smmintrin.h>
#include "definitions.h"

void svt_av1_calc_indices_dim1_sse4_1(const int* data, const int* centroids, uint8_t* indices, int n, int k) {
    int results[MAX_SB_SQUARE];
    memset(indices, 0, n * sizeof(uint8_t));

    __m128i c0 = _mm_set1_epi32(centroids[0]);
    for (int i = 0; i < n; i += 4) {
        __m128i sub = _mm_sub_epi32(_mm_loadu_si128((const __m128i*)(data + i)), c0);
        _mm_storeu_si128((__m128i*)(results + i), _mm_mullo_epi32(sub, sub));
    }

    for (int c = 1; c < k; c++) {
        const __m128i cent  = _mm_set1_epi32(centroids[c]);
        const __m128i idx_v = _mm_set1_epi32(c);
        for (int i = 0; i < n; i += 8) {
            const __m128i d1   = _mm_loadu_si128((const __m128i*)(data + i));
            const __m128i d2   = _mm_loadu_si128((const __m128i*)(data + i + 4));
            const __m128i s1   = _mm_sub_epi32(d1, cent);
            const __m128i s2   = _mm_sub_epi32(d2, cent);
            const __m128i dst1 = _mm_mullo_epi32(s1, s1);
            const __m128i dst2 = _mm_mullo_epi32(s2, s2);

            const __m128i prev1 = _mm_loadu_si128((const __m128i*)(results + i));
            const __m128i prev2 = _mm_loadu_si128((const __m128i*)(results + i + 4));
            const __m128i cmp1  = _mm_cmpgt_epi32(prev1, dst1);
            const __m128i cmp2  = _mm_cmpgt_epi32(prev2, dst2);

            _mm_storeu_si128((__m128i*)(results + i), _mm_blendv_epi8(prev1, dst1, cmp1));
            _mm_storeu_si128((__m128i*)(results + i + 4), _mm_blendv_epi8(prev2, dst2, cmp2));

            const __m128i iv1  = _mm_and_si128(idx_v, cmp1);
            const __m128i iv2  = _mm_and_si128(idx_v, cmp2);
            const __m128i i16  = _mm_packus_epi32(iv1, iv2);
            const __m128i i8   = _mm_packs_epi16(i16, _mm_setzero_si128());
            const __m128i load = _mm_loadl_epi64((const __m128i*)(indices + i));
            _mm_storel_epi64((__m128i*)(indices + i), _mm_max_epi8(load, i8));
        }
    }
}
