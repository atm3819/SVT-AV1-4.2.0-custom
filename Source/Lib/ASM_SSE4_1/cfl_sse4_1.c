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

static INLINE __m128i predict_unclipped_sse4_1(const __m128i* input, __m128i alpha_q12, __m128i alpha_sign,
                                               __m128i dc_q0) {
    __m128i ac_q3          = _mm_loadu_si128(input);
    __m128i ac_sign        = _mm_sign_epi16(alpha_sign, ac_q3);
    __m128i scaled_luma_q0 = _mm_mulhrs_epi16(_mm_abs_epi16(ac_q3), alpha_q12);
    scaled_luma_q0         = _mm_sign_epi16(scaled_luma_q0, ac_sign);
    return _mm_add_epi16(scaled_luma_q0, dc_q0);
}

void svt_cfl_predict_lbd_sse4_1(const int16_t* pred_buf_q3, uint8_t* pred, int32_t pred_stride, uint8_t* dst,
                                int32_t dst_stride, int32_t alpha_q3, int32_t bit_depth, int32_t width,
                                int32_t height) {
    (void)bit_depth;
    (void)pred_stride;
    const __m128i        alpha_sign = _mm_set1_epi16((int16_t)alpha_q3);
    const __m128i        alpha_q12  = _mm_slli_epi16(_mm_abs_epi16(alpha_sign), 9);
    const __m128i        dc_q0      = _mm_set1_epi16(*pred);
    const __m128i*       row        = (const __m128i*)pred_buf_q3;
    const __m128i* const row_end    = row + height * CFL_BUF_LINE_I128;
    do {
        if (width < 16) {
            __m128i res = predict_unclipped_sse4_1(row, alpha_q12, alpha_sign, dc_q0);
            res         = _mm_packus_epi16(res, res);
            if (width == 4) {
                const int v = _mm_cvtsi128_si32(res);
                memcpy(dst, &v, 4);
            } else {
                _mm_storel_epi64((__m128i*)dst, res);
            }
        } else {
            for (int32_t x = 0; x < width; x += 16) {
                const __m128i a = predict_unclipped_sse4_1(row + (x >> 3), alpha_q12, alpha_sign, dc_q0);
                const __m128i b = predict_unclipped_sse4_1(row + (x >> 3) + 1, alpha_q12, alpha_sign, dc_q0);
                _mm_storeu_si128((__m128i*)(dst + x), _mm_packus_epi16(a, b));
            }
        }
        dst += dst_stride;
        row += CFL_BUF_LINE_I128;
    } while (row < row_end);
}
