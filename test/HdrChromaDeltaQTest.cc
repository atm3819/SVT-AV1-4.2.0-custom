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
 * mapping (svt_aom_get_hdr_chroma_dqp).
 *
 * The mapping uses the SVT-AV1/libaom shared relation qindex = 4 * QP: the base
 * QP is base_q_idx / 4, the H.Sup15 chroma QP offset is evaluated there, and the
 * result is converted back to a qindex delta by * 4. The whole thing is a small
 * closed form, so the reference below is hand-verifiable (no LUT, no oracle).
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

// Reference implementation of the H.Sup15 chroma QP offset under qindex = 4 * QP:
//   base_qp   = qindex / 4
//   chroma_qp = -0.46 * base_qp + 9.26
//   d         = 1.04 * chroma_qp * 4        (QP offset scaled into the qindex domain)
//   round half away from zero; min(0, d); clip to [-48, 0]  (the [-12, 0] QP clamp * 4)
int32_t reference_dqp(int32_t qindex) {
    const double base_qp   = qindex / 4.0;
    const double chroma_qp = -0.46 * base_qp + 9.26;
    const double d_fp      = 1.04 * chroma_qp * 4.0;
    int32_t      d         = (int32_t)(d_fp + (d_fp < 0 ? -0.5 : 0.5));
    if (d > 0)
        d = 0;
    if (d < -48)
        d = -48;
    return d;
}

TEST(HdrChromaDeltaQTest, MatchesReferenceForAllQindex) {
    for (int32_t q = 0; q < 256; q++) {
        EXPECT_EQ(svt_aom_get_hdr_chroma_dqp(q, kPlaneU), reference_dqp(q)) << "qindex " << q;
    }
}

TEST(HdrChromaDeltaQTest, GoldenValues) {
    // Hand-computed anchors (verifiable with a calculator):
    //   q= 80: base_qp 20.00, chroma_qp  0.06, d_fp +0.25 -> 0
    //   q= 82: base_qp 20.50, chroma_qp -0.17, d_fp -0.71 -> -1
    //   q=100: base_qp 25.00, chroma_qp -2.24, d_fp -9.32 -> -9
    //   q=128: base_qp 32.00, chroma_qp -5.46, d_fp -22.7 -> -23
    //   q=200: base_qp 50.00, chroma_qp -13.74, d_fp -57.2 -> clip -48
    //   q=255: base_qp 63.75, chroma_qp -20.07, d_fp -83.5 -> clip -48
    const struct {
        int32_t qindex;
        int32_t expected;
    } golden[] = {
        {0, 0}, {32, 0}, {80, 0}, {82, -1}, {100, -9}, {128, -23}, {200, -48}, {255, -48},
    };
    for (const auto& g : golden) {
        EXPECT_EQ(svt_aom_get_hdr_chroma_dqp(g.qindex, kPlaneU), g.expected) << "qindex " << g.qindex;
    }
}

TEST(HdrChromaDeltaQTest, RangeAndMonotonicity) {
    int32_t prev = 0;
    for (int32_t q = 0; q < 256; q++) {
        const int32_t d = svt_aom_get_hdr_chroma_dqp(q, kPlaneU);
        EXPECT_LE(d, 0) << "qindex " << q;
        EXPECT_GE(d, -48) << "qindex " << q;
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
        EXPECT_EQ(svt_aom_get_hdr_chroma_dqp(q, kPlaneU), svt_aom_get_hdr_chroma_dqp(q, kPlaneV)) << "qindex " << q;
    }
}

}  // namespace
