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

#include <arm_neon.h>
#include "definitions.h"
#include "mem_neon.h"

extern double similarity(uint32_t sum_s, uint32_t sum_r, uint32_t sum_sq_s, uint32_t sum_sq_r, uint32_t sum_sxr,
                         int count, uint32_t bd);

double svt_ssim_8x8_neon(const uint8_t* s, uint32_t sp, const uint8_t* r, uint32_t rp) {
    uint32x4_t vsum_s = vdupq_n_u32(0), vsum_r = vdupq_n_u32(0), vsum_sq_s = vdupq_n_u32(0), vsum_sq_r = vdupq_n_u32(0),
               vsum_sxr = vdupq_n_u32(0);
    int        i        = 8;
    do {
        const uint8x8_t vs = vld1_u8(s);
        const uint8x8_t vr = vld1_u8(r);
        vsum_s             = vpadalq_u16(vsum_s, vmovl_u8(vs));
        vsum_r             = vpadalq_u16(vsum_r, vmovl_u8(vr));
        vsum_sq_s          = vpadalq_u16(vsum_sq_s, vmull_u8(vs, vs));
        vsum_sq_r          = vpadalq_u16(vsum_sq_r, vmull_u8(vr, vr));
        vsum_sxr           = vpadalq_u16(vsum_sxr, vmull_u8(vs, vr));
        s += sp;
        r += rp;
    } while (--i != 0);
    return similarity(vaddvq_u32(vsum_s),
                      vaddvq_u32(vsum_r),
                      vaddvq_u32(vsum_sq_s),
                      vaddvq_u32(vsum_sq_r),
                      vaddvq_u32(vsum_sxr),
                      64,
                      8);
}

double svt_ssim_4x4_neon(const uint8_t* s, uint32_t sp, const uint8_t* r, uint32_t rp) {
    uint32x4_t vsum_s = vdupq_n_u32(0), vsum_r = vdupq_n_u32(0), vsum_sq_s = vdupq_n_u32(0), vsum_sq_r = vdupq_n_u32(0),
               vsum_sxr = vdupq_n_u32(0);
    int        i        = 4;
    do {
        const uint8x8_t vs = load_u8_4x1(s);
        const uint8x8_t vr = load_u8_4x1(r);
        vsum_s             = vpadalq_u16(vsum_s, vmovl_u8(vs));
        vsum_r             = vpadalq_u16(vsum_r, vmovl_u8(vr));
        vsum_sq_s          = vpadalq_u16(vsum_sq_s, vmull_u8(vs, vs));
        vsum_sq_r          = vpadalq_u16(vsum_sq_r, vmull_u8(vr, vr));
        vsum_sxr           = vpadalq_u16(vsum_sxr, vmull_u8(vs, vr));
        s += sp;
        r += rp;
    } while (--i != 0);
    return similarity(vaddvq_u32(vsum_s),
                      vaddvq_u32(vsum_r),
                      vaddvq_u32(vsum_sq_s),
                      vaddvq_u32(vsum_sq_r),
                      vaddvq_u32(vsum_sxr),
                      16,
                      8);
}

double svt_ssim_8x8_hbd_neon(const uint16_t* s, uint32_t sp, const uint16_t* r, uint32_t rp) {
    uint32x4_t vsum_s = vdupq_n_u32(0), vsum_r = vdupq_n_u32(0), vsum_sq_s = vdupq_n_u32(0), vsum_sq_r = vdupq_n_u32(0),
               vsum_sxr = vdupq_n_u32(0);
    int        i        = 8;
    do {
        const uint16x8_t vs = vld1q_u16(s);
        const uint16x8_t vr = vld1q_u16(r);
        vsum_s              = vpadalq_u16(vsum_s, vs);
        vsum_r              = vpadalq_u16(vsum_r, vr);
        vsum_sq_s           = vmlal_u16(vsum_sq_s, vget_low_u16(vs), vget_low_u16(vs));
        vsum_sq_s           = vmlal_high_u16(vsum_sq_s, vs, vs);
        vsum_sq_r           = vmlal_u16(vsum_sq_r, vget_low_u16(vr), vget_low_u16(vr));
        vsum_sq_r           = vmlal_high_u16(vsum_sq_r, vr, vr);
        vsum_sxr            = vmlal_u16(vsum_sxr, vget_low_u16(vs), vget_low_u16(vr));
        vsum_sxr            = vmlal_high_u16(vsum_sxr, vs, vr);
        s += sp;
        r += rp;
    } while (--i != 0);
    return similarity(vaddvq_u32(vsum_s),
                      vaddvq_u32(vsum_r),
                      vaddvq_u32(vsum_sq_s),
                      vaddvq_u32(vsum_sq_r),
                      vaddvq_u32(vsum_sxr),
                      64,
                      10);
}

double svt_ssim_4x4_hbd_neon(const uint16_t* s, uint32_t sp, const uint16_t* r, uint32_t rp) {
    uint32x4_t vsum_s = vdupq_n_u32(0), vsum_r = vdupq_n_u32(0), vsum_sq_s = vdupq_n_u32(0), vsum_sq_r = vdupq_n_u32(0),
               vsum_sxr = vdupq_n_u32(0);
    int        i        = 4;
    do {
        const uint16x4_t vs = vld1_u16(s);
        const uint16x4_t vr = vld1_u16(r);
        vsum_s              = vaddw_u16(vsum_s, vs);
        vsum_r              = vaddw_u16(vsum_r, vr);
        vsum_sq_s           = vmlal_u16(vsum_sq_s, vs, vs);
        vsum_sq_r           = vmlal_u16(vsum_sq_r, vr, vr);
        vsum_sxr            = vmlal_u16(vsum_sxr, vs, vr);
        s += sp;
        r += rp;
    } while (--i != 0);
    return similarity(vaddvq_u32(vsum_s),
                      vaddvq_u32(vsum_r),
                      vaddvq_u32(vsum_sq_s),
                      vaddvq_u32(vsum_sq_r),
                      vaddvq_u32(vsum_sxr),
                      16,
                      10);
}
