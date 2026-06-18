/*
 * Copyright(c) 2026 Meta Platforms, Inc.
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
 * @file SvtAv1KeyFramePacingTest.cc
 *
 * @brief End-to-end test for RTC key-frame pacing (key_frame_min_interval +
 *        forced_kf_policy), the libaom kf_min_dist-style floor.
 *
 * Encodes a flat-IPP LOW_DELAY CBR clip and drives application-forced
 * key-frame requests (input pic_type == EB_AV1_KEY_PICTURE) on selected
 * frames. The encoder stamps output pic_type == EB_AV1_KEY_PICTURE on each
 * key-frame packet (packetization_process.c), so we count key frames directly
 * from the output stream — no decode needed.
 *
 * Matrix (forced requests on frames {10,11,12,13}, opener always a key frame):
 *   - min_interval=0 (disabled)        -> every request honored: {0,10,11,12,13}
 *   - min_interval=6, policy HONOR(0)  -> floor ignored for forced: {0,10,11,12,13}
 *   - min_interval=6, policy COALESCE(1) -> burst collapsed:        {0,10}
 *   - min_interval=6, policy DEFER(2)  -> request paced to floor:   {0,10,16}
 * Plus: periodic key frames (--keyint) are NEVER gated by the floor.
 ******************************************************************************/

#include <algorithm>
#include <cstdint>
#include <set>
#include <vector>
#include "DummyVideoSource.h"
#include "EbSvtAv1.h"
#include "EbSvtAv1Enc.h"
#include "gtest/gtest.h"

namespace {

constexpr uint32_t kWidth = 320;
constexpr uint32_t kHeight = 240;

struct PacingParams {
    uint32_t num_frames;
    uint32_t key_frame_min_interval;
    uint8_t forced_kf_policy;
    uint32_t intra_period_length;  // large => periodic KFs out of the way
    std::set<uint32_t> forced_frames;  // input frames flagged EB_AV1_KEY_PICTURE
};

// Encode the clip and return the display-order positions (pts) of every
// key-frame packet the encoder emitted. The opener is always a key frame.
static std::vector<uint64_t> encode_collect_keyframes(const PacingParams& p) {
    EbComponentType* enc = nullptr;
    EbSvtAv1EncConfiguration cfg{};
    EXPECT_EQ(EB_ErrorNone, svt_av1_enc_init_handle(&enc, &cfg));

    cfg.source_width = kWidth;
    cfg.source_height = kHeight;
    cfg.frame_rate_numerator = 30000;
    cfg.frame_rate_denominator = 1000;
    cfg.encoder_bit_depth = 8;
    cfg.encoder_color_format = EB_YUV420;
    cfg.enc_mode = 9;
    cfg.tune = 1;
    cfg.rtc = true;
    cfg.intra_period_length = (int32_t)p.intra_period_length;
    cfg.hierarchical_levels = 0;
    cfg.pred_structure = LOW_DELAY;
    cfg.rate_control_mode = SVT_AV1_RC_MODE_CBR;
    cfg.target_bit_rate = 500000;
    cfg.max_qp_allowed = 63;
    cfg.min_qp_allowed = 4;
    cfg.look_ahead_distance = 0;
    cfg.recode_loop = 0;
    cfg.scene_change_detection = 0;  // deterministic: only forced/periodic KFs
    cfg.key_frame_min_interval = p.key_frame_min_interval;
    cfg.forced_kf_policy = p.forced_kf_policy;

    EXPECT_EQ(EB_ErrorNone, svt_av1_enc_set_parameter(enc, &cfg));
    EXPECT_EQ(EB_ErrorNone, svt_av1_enc_init(enc));

    svt_av1_video_source::DummyVideoSource src(IMG_FMT_420, kWidth, kHeight, 8);
    EXPECT_EQ(0, src.open_source(0, 0));

    std::vector<uint64_t> kf_pts;

    EbBufferHeaderType in_hdr{};
    in_hdr.size = sizeof(EbBufferHeaderType);

    for (uint32_t fi = 0; fi < p.num_frames; ++fi) {
        EbSvtIOFormat* frame = src.get_next_frame();
        EXPECT_NE(nullptr, frame);

        in_hdr.p_buffer = reinterpret_cast<uint8_t*>(frame);
        in_hdr.n_filled_len = src.get_frame_size();
        in_hdr.pts = fi;
        in_hdr.flags = 0;
        const bool force = (fi == 0) || p.forced_frames.count(fi) != 0;
        in_hdr.pic_type = force ? EB_AV1_KEY_PICTURE : EB_AV1_INVALID_PICTURE;
        in_hdr.p_app_private = nullptr;

        EXPECT_EQ(EB_ErrorNone, svt_av1_enc_send_picture(enc, &in_hdr))
            << "send_picture failed on frame " << fi;

        EbBufferHeaderType* out = nullptr;
        EbErrorType rc = svt_av1_enc_get_packet(enc, &out, 0);
        if (rc == EB_ErrorNone && out) {
            if (out->pic_type == EB_AV1_KEY_PICTURE)
                kf_pts.push_back(out->pts);
            svt_av1_enc_release_out_buffer(&out);
        }
    }

    EbBufferHeaderType eos{};
    eos.size = sizeof(EbBufferHeaderType);
    eos.flags = EB_BUFFERFLAG_EOS;
    eos.pic_type = EB_AV1_INVALID_PICTURE;
    EXPECT_EQ(EB_ErrorNone, svt_av1_enc_send_picture(enc, &eos));
    for (;;) {
        EbBufferHeaderType* out = nullptr;
        EbErrorType rc = svt_av1_enc_get_packet(enc, &out, 1);
        if (rc != EB_ErrorNone || !out)
            break;
        const bool is_eos = (out->flags & EB_BUFFERFLAG_EOS) != 0;
        if (!is_eos && out->n_filled_len > 0 &&
            out->pic_type == EB_AV1_KEY_PICTURE)
            kf_pts.push_back(out->pts);
        svt_av1_enc_release_out_buffer(&out);
        if (is_eos)
            break;
    }

    svt_av1_enc_deinit(enc);
    svt_av1_enc_deinit_handle(enc);

    std::sort(kf_pts.begin(), kf_pts.end());
    return kf_pts;
}

// Convenience: a burst of forced requests on consecutive frames 10..13.
static std::set<uint32_t> burst_10_to_13() { return {10u, 11u, 12u, 13u}; }

}  // namespace

// Floor disabled (default) => behavior is legacy: every forced request honored.
TEST(KeyFramePacing, DisabledHonorsEveryForcedRequest) {
    PacingParams p{40, /*min*/ 0, /*policy*/ 0, /*intra*/ 1000, burst_10_to_13()};
    EXPECT_EQ((std::vector<uint64_t>{0, 10, 11, 12, 13}),
              encode_collect_keyframes(p));
}

// policy HONOR: the floor never suppresses an application-forced request
// (matches libaom, where AOM_EFLAG_FORCE_KF overrides kf_min_dist).
TEST(KeyFramePacing, HonorIgnoresFloorForForcedRequests) {
    PacingParams p{40, /*min*/ 6, /*policy*/ 0, /*intra*/ 1000, burst_10_to_13()};
    EXPECT_EQ((std::vector<uint64_t>{0, 10, 11, 12, 13}),
              encode_collect_keyframes(p));
}

// policy COALESCE: a too-soon request is dropped; the burst collapses to the
// single key frame that already fired at frame 10.
TEST(KeyFramePacing, CoalesceCollapsesBurst) {
    PacingParams p{40, /*min*/ 6, /*policy*/ 1, /*intra*/ 1000, burst_10_to_13()};
    EXPECT_EQ((std::vector<uint64_t>{0, 10}), encode_collect_keyframes(p));
}

// policy DEFER: the request is preserved but postponed to the first frame that
// is >= key_frame_min_interval from the previous key frame (10 + 6 = 16).
TEST(KeyFramePacing, DeferPostponesToFloorBoundary) {
    PacingParams p{40, /*min*/ 6, /*policy*/ 2, /*intra*/ 1000, burst_10_to_13()};
    EXPECT_EQ((std::vector<uint64_t>{0, 10, 16}), encode_collect_keyframes(p));
}

// The floor must never gate periodic key frames driven by intra_period_length,
// even when key_frame_min_interval is larger than the period. With
// intra_period_length=8 the encoder emits a key frame every 9 frames; an
// aggressive min interval of 20 must not suppress any of them.
TEST(KeyFramePacing, PeriodicKeyFramesAreNeverGated) {
    PacingParams p{40, /*min*/ 20, /*policy*/ 1, /*intra period*/ 8, /*forced*/ {}};
    EXPECT_EQ((std::vector<uint64_t>{0, 9, 18, 27, 36}),
              encode_collect_keyframes(p));
}

// Adjacent-gap invariant under COALESCE/DEFER: no two key frames closer than
// the floor (the opener excepted, which is unconditional).
TEST(KeyFramePacing, EnforcesMinGapInvariant) {
    for (uint8_t policy : {uint8_t{1}, uint8_t{2}}) {
        PacingParams p{40, /*min*/ 6, policy, /*intra*/ 1000, burst_10_to_13()};
        const auto kf = encode_collect_keyframes(p);
        for (size_t i = 1; i < kf.size(); ++i)
            EXPECT_GE(kf[i] - kf[i - 1], 6u)
                << "policy " << (int)policy << ": key frames at " << kf[i - 1]
                << " and " << kf[i] << " violate min interval 6";
    }
}
