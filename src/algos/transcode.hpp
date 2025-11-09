#pragma once
#include "formatcontext.hpp"
#include "codeccontext.hpp"
#include <libavcodec/codec_par.h>
#include <libavutil/rational.h>
#include <memory>
#include "audiofifo.hpp"
#include "stream.hpp"
#include "swrcontext.hpp"
extern "C" {
    #include <libavcodec/packet.h>
    #include <libavutil/mathematics.h>
    #include <libavutil/channel_layout.h>
    #include <libavutil/frame.h>
    #include <libavutil/samplefmt.h>
    #include <libswresample/swresample.h>
    #include <libavutil/opt.h>
    #include <libavutil/audio_fifo.h>
}

struct IContext {
    // AVFormatContext* fmt_ctx{nullptr};
    InputFormatContext fmt_ctx;

    std::string filepath{};

    int astream_idx{};
    int vstream_idx{};

    bool mux_audio;
    bool mux_video;

    std::unique_ptr<CodecContext> audio_ctx{nullptr};
    std::unique_ptr<CodecContext> video_ctx{nullptr};

    AVStream* audio_stream{nullptr};
    AVStream* video_stream{nullptr};

    const AVCodec* audio_codec{nullptr};
    const AVCodec* video_codec{nullptr};
};
struct OContext {
    OutputFormatContext fmt_ctx;

    std::string filepath{};

    int astream_idx{};
    int vstream_idx{};

    std::unique_ptr<CodecContext> audio_ctx{nullptr};
    std::unique_ptr<CodecContext> video_ctx{nullptr};

    StreamT audio_stream{nullptr};
    StreamT video_stream{nullptr};

    const AVCodec* audio_codec{nullptr};
    const AVCodec* video_codec{nullptr};
};

#define handle_transcode_error(cond, msg) if (cond) { throw transcoding_error(msg); }

void transcode(IContext& ictx, OContext& octx, std::string_view codec_audio, std::string_view codec_video);
void flush(IContext& ictx, OContext& octx, std::unique_ptr<SwrCtx> swr_ctx, std::unique_ptr<AudioFifo> fifo, int64_t pts);
