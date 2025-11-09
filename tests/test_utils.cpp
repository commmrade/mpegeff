#include <gtest/gtest.h>
#include <cstdint>

#include "../src/utils.hpp"
#include "../src/types/codeccontext.hpp"

extern "C" {
#include <libavutil/channel_layout.h>
}

// Helper to initialize a CodecContext for tests.
// Passing nullptr to the constructor is acceptable here because we don't
// rely on a specific AVCodec for the branches we test (aac/default).
static void init_codec_ctx_for_test(CodecContext &ctx, int sample_rate, int channels) {
    AVCodecContext *c = ctx.get_inner();
    ASSERT_NE(c, nullptr);
    c->sample_rate = sample_rate;
    // Initialize a minimal channel layout: set number of channels so copy can be tested.
    c->ch_layout.nb_channels = channels;
    // For simplicity, ensure time_base has sane non-zero denominator so tests around time_base are deterministic.
    if (c->sample_rate == 0) {
        c->sample_rate = 44100;
    }
}

/*
 * Test cases:
 * - For encoder == "aac":
 *   * occtx.bit_rate should be set to 128000
 *   * occtx.sample_rate should be set to icctx.sample_rate
 *   * occtx.sample_fmt should be AV_SAMPLE_FMT_FLTP
 *   * channel layout should be copied from icctx to occtx
 *   * time_base should be set to 1 / original occtx.sample_rate (note: function uses pre-existing occtx.sample_rate)
 *
 * - For a non-opus, non-aac encoder (default branch), behavior is similar to 'aac' in this implementation.
 */

TEST(SetAudioCodecParamsTest, AAC_SetsExpectedFields) {
    CodecContext occtx(nullptr);
    CodecContext icctx(nullptr);

    // Initialize contexts so time_base computation is well-defined.
    init_codec_ctx_for_test(occtx, 44100, 1); // original occtx sample_rate used when computing time_base
    init_codec_ctx_for_test(icctx, 48000, 2); // icctx sample_rate should be applied to occtx

    // Pre-condition checks
    EXPECT_EQ(occtx.get_inner()->sample_rate, 44100);
    EXPECT_EQ(icctx.get_inner()->sample_rate, 48000);

    // Call under test. 'codec' argument is not used in the 'aac' branch, so pass nullptr.
    set_audio_codec_params(nullptr, occtx, icctx, "aac");

    // Assertions
    EXPECT_EQ(occtx.get_inner()->bit_rate, 128000);
    EXPECT_EQ(occtx.get_inner()->sample_rate, icctx.get_inner()->sample_rate);
    EXPECT_EQ(occtx.get_inner()->sample_fmt, AV_SAMPLE_FMT_FLTP);

    // time_base was set to 1 / original occtx sample_rate (44100)
    EXPECT_EQ(occtx.get_inner()->time_base.num, 1);
    EXPECT_EQ(occtx.get_inner()->time_base.den, 44100);

    // channel layout copy check: nb_channels should match
    EXPECT_EQ(occtx.get_inner()->ch_layout.nb_channels, icctx.get_inner()->ch_layout.nb_channels);
}

TEST(SetAudioCodecParamsTest, DefaultEncoder_BehavesLikeAACForTheseFields) {
    CodecContext occtx(nullptr);
    CodecContext icctx(nullptr);

    init_codec_ctx_for_test(occtx, 32000, 2);
    init_codec_ctx_for_test(icctx, 44100, 1);

    // Ensure different initial rates to validate change
    EXPECT_EQ(occtx.get_inner()->sample_rate, 32000);
    EXPECT_EQ(icctx.get_inner()->sample_rate, 44100);

    set_audio_codec_params(nullptr, occtx, icctx, "vorbis"); // 'vorbis' goes to default branch

    EXPECT_EQ(occtx.get_inner()->bit_rate, 128000);
    EXPECT_EQ(occtx.get_inner()->sample_fmt, AV_SAMPLE_FMT_FLTP);
    EXPECT_EQ(occtx.get_inner()->sample_rate, 44100);

    // time_base was set using original occtx sample_rate (32000)
    EXPECT_EQ(occtx.get_inner()->time_base.num, 1);
    EXPECT_EQ(occtx.get_inner()->time_base.den, 32000);

    EXPECT_EQ(occtx.get_inner()->ch_layout.nb_channels, icctx.get_inner()->ch_layout.nb_channels);
}