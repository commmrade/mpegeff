#pragma once
#include "codeccontext.hpp"
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <stdexcept>
#include <string_view>

void set_audio_codec_params(const AVCodec* codec, CodecContext& occtx, CodecContext& icctx, std::string_view encoder) {
    if (encoder == "aac") {
        occtx.get_inner()->bit_rate = 128000;  // 128 kbps для хорошего качества
        occtx.get_inner()->time_base = AVRational{1, occtx.get_inner()->sample_rate};
        occtx.get_inner()->sample_rate = icctx.get_inner()->sample_rate;

        occtx.get_inner()->sample_fmt = AV_SAMPLE_FMT_FLTP;
        av_channel_layout_copy(&occtx.get_inner()->ch_layout, &icctx.get_inner()->ch_layout);
    } else if (encoder == "opus" || encoder == "libopus") {
        occtx.get_inner()->bit_rate = 128000;  // 128 kbps для хорошего качества
        occtx.get_inner()->time_base = AVRational{1, occtx.get_inner()->sample_rate};
        // occtx.get_inner()->sample_rate = icctx.get_inner()->sample_rate;
        // occtx.get_inner()->sample_rate = codec->supported_samplerates[0];

        const int* supported = nullptr;
        int count = 0;

        int r = avcodec_get_supported_config(
            nullptr,
            codec,
            AV_CODEC_CONFIG_SAMPLE_RATE,
            0,
            (const void**)&supported,
            &count
        );
        if (r < 0 || !supported || count <= 0) {
            throw std::runtime_error("Could not get supported sample rates");
        }
        int chosen_rate = supported[0];
        occtx.get_inner()->sample_rate = chosen_rate;

        occtx.get_inner()->sample_fmt = AV_SAMPLE_FMT_S16;
        av_channel_layout_copy(&occtx.get_inner()->ch_layout, &icctx.get_inner()->ch_layout);
    } else {
        occtx.get_inner()->bit_rate = 128000;  // 128 kbps для хорошего качества
        occtx.get_inner()->time_base = AVRational{1, occtx.get_inner()->sample_rate};
        occtx.get_inner()->sample_rate = icctx.get_inner()->sample_rate;

        occtx.get_inner()->sample_fmt = AV_SAMPLE_FMT_FLTP;  // Float planar для высокого качества
        av_channel_layout_copy(&occtx.get_inner()->ch_layout, &icctx.get_inner()->ch_layout);
    }
}
