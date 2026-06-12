/*
 * Copyright(c) 2026 Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at https://www.aomedia.org/license/software-license. If the
 * Alliance for Open Media Patent License 1.0 was not distributed with this
 * source code in the PATENTS file, you can obtain it at
 * https://www.aomedia.org/license/patent-license.
 */

/******************************************************************************
 * @file HdrChromaDeltaQTest.cc
 *
 * @brief Unit test for the ITU-T H.Sup15 section 8.3.2 HDR chroma delta-q
 * mapping (svt_aom_get_hdr_chroma_dqp), ported from libaom
 * (av1/encoder/av1_quantize.c adjust_hdr_cb_deltaq()/adjust_hdr_cr_deltaq()).
 *
 ******************************************************************************/

#include <stdint.h>
#include "EbDebugMacros.h"
#include "gtest/gtest.h"

extern "C" {
int8_t svt_aom_get_hdr_chroma_dqp(int32_t base_q_idx, int32_t plane);
}

namespace {

// Matches the Plane enum in Source/Lib/Codec/definitions.h
const int32_t kPlaneU = 1;
const int32_t kPlaneV = 2;

#if HDR_CHROMA_DQP_USE_LUT

TEST(HdrChromaDeltaQTest, GoldenValues) {
    // Spot values of hdr_chroma_dqp_lut (rc_process.c): the H.Sup15 chroma QP
    // offset evaluated at the equivalent QP derived from the 10-bit AC dequant
    // step table, converted back to a qindex delta (see the LUT comment there)
    const struct {
        int32_t qindex;
        int32_t expected;
    } golden[] = {
        {0, 0},      // no offset in the low-qindex region ...
        {32, 0},     //
        {56, 0},     // ... up to and including qindex 56
        {57, -6},    // first offset step
        {64, -6},    //
        {70, -7},    //
        {71, -14},   // integer-QP step over a steep QP_eq region
        {89, -18},   //
        {90, -24},   // saturation point
        {255, -24},  //
    };
    for (const auto& g : golden) {
        EXPECT_EQ(svt_aom_get_hdr_chroma_dqp(g.qindex, kPlaneU), g.expected)
            << "qindex " << g.qindex;
    }
}

#else  // formula path

// Reference implementation of the libaom formula:
// baseQp = qindex / 2.0; chromaQp = -0.46 * baseQp + 9.26;
// d = 1.04 * chromaQp * 2.0; round half away from zero; min(0, d);
// clip to [-24, 24]
int32_t reference_dqp(int32_t qindex) {
    const double base_qp = qindex / 2.0;
    const double chroma_qp = -0.46 * base_qp + 9.26;
    const double d_fp = 1.04 * chroma_qp * 2.0;
    int32_t d = (int32_t)(d_fp + (d_fp < 0 ? -0.5 : 0.5));
    if (d > 0)
        d = 0;
    if (d < -24)
        d = -24;
    return d;
}

TEST(HdrChromaDeltaQTest, GoldenValues) {
    // Hand-computed from the libaom formula
    const struct {
        int32_t qindex;
        int32_t expected;
    } golden[] = {
        {1, 0},      // positive mapping result, capped to 0
        {32, 0},     // still in the capped region
        {56, -8},    // first steep region
        {64, -11},   //
        {100, -24},  // saturates at the clip
        {128, -24},  //
        {200, -24},  //
        {255, -24},  //
    };
    for (const auto& g : golden) {
        EXPECT_EQ(svt_aom_get_hdr_chroma_dqp(g.qindex, kPlaneU), g.expected)
            << "qindex " << g.qindex;
    }
}

TEST(HdrChromaDeltaQTest, MatchesReferenceForAllQindex) {
    for (int32_t q = 0; q < 256; q++) {
        EXPECT_EQ(svt_aom_get_hdr_chroma_dqp(q, kPlaneU), reference_dqp(q))
            << "qindex " << q;
    }
}

#endif  // HDR_CHROMA_DQP_USE_LUT

TEST(HdrChromaDeltaQTest, RangeAndMonotonicity) {
    int32_t prev = 0;
    for (int32_t q = 0; q < 256; q++) {
        const int32_t d = svt_aom_get_hdr_chroma_dqp(q, kPlaneU);
        EXPECT_LE(d, 0) << "qindex " << q;
        EXPECT_GE(d, -24) << "qindex " << q;
        if (q > 0) {
            // The mapping is non-increasing in qindex
            EXPECT_LE(d, prev) << "qindex " << q;
        }
        prev = d;
    }
}

TEST(HdrChromaDeltaQTest, CbEqualsCrWithCurrentConstants) {
    // The Cb and Cr scale constants are currently identical; the sequence
    // header separate_uv_delta_q derivation and encode_quantization() rely on
    // the resulting deltas being equal. This test must be revisited if
    // per-component constants are introduced.
    for (int32_t q = 0; q < 256; q++) {
        EXPECT_EQ(svt_aom_get_hdr_chroma_dqp(q, kPlaneU),
                  svt_aom_get_hdr_chroma_dqp(q, kPlaneV))
            << "qindex " << q;
    }
}

}  // namespace
