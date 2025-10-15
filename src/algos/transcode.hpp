#pragma once
#include "formatcontext.hpp"
#include "codeccontext.hpp"
#include <format>

struct IContext {
    // AVFormatContext* fmt_ctx{nullptr};
    InputFormatContext fmt_ctx;

    std::string filepath{};

    int astream_idx{};
    int vstream_idx{};

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

void transcode(std::string_view input, std::string_view output, IContext& ictx, OContext& octx) {
    ictx.fmt_ctx.open_input(ictx.filepath);
    ictx.fmt_ctx.find_best_stream_info();

    int r = 0;
    std::vector<AVStream*> istreams = ictx.fmt_ctx.streams();
    for (auto i = 0; i < istreams.size(); ++i) {
        AVCodecParameters* cparams = istreams[i]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(cparams->codec_id);
        handle_transcode_error(!codec, std::format("Could not find decoder for stream {}", i));

        if (cparams->codec_type == AVMEDIA_TYPE_AUDIO) {
            ictx.astream_idx = i;
            ictx.audio_codec = codec;

            ictx.audio_ctx = std::make_unique<CodecContext>(codec);
            ictx.audio_ctx->copy_params_from(cparams);
            ictx.audio_ctx->open(codec);
        } else if (cparams->codec_type == AVMEDIA_TYPE_VIDEO) {
            ictx.vstream_idx = i;
            ictx.video_codec = codec;

            ictx.video_ctx = std::make_unique<CodecContext>(codec);
            ictx.video_ctx->copy_params_from(cparams);
            ictx.video_ctx->open(codec);
        }
    }

    octx.fmt_ctx.alloc_output(octx.filepath);

    // Setup encoder
    for (auto i = 0; i < istreams.size(); ++i) {
        AVStream* i_stream = istreams[i];
        int media_type = i_stream->codecpar->codec_type;
        if (media_type == AVMEDIA_TYPE_AUDIO) {
            StreamT o_stream = octx.fmt_ctx.new_stream();
            octx.audio_stream = o_stream;
            octx.astream_idx = i;
            avcodec_parameters_copy(o_stream->codecpar, i_stream->codecpar);
        } else if (media_type == AVMEDIA_TYPE_VIDEO) {
            octx.vstream_idx = i;
            StreamT o_stream = octx.fmt_ctx.new_stream();
            octx.video_stream = o_stream;
            
            const char* encoder_name = "libx265"; // TODO: CHange
            const AVCodec* codec = avcodec_find_encoder_by_name("libx265");
            handle_transcode_error(!codec, std::format("Could not find encoder by name {}", encoder_name));

            octx.video_codec = codec;

            octx.video_ctx = std::make_unique<CodecContext>(codec);            
            octx.video_ctx->get_inner()->bit_rate = ictx.video_ctx->get_inner()->bit_rate;
            octx.video_ctx->get_inner()->time_base = av_inv_q(ictx.video_ctx->get_inner()->framerate);
            octx.video_ctx->get_inner()->height = ictx.video_ctx->get_inner()->height;
            octx.video_ctx->get_inner()->width = ictx.video_ctx->get_inner()->width;
            octx.video_ctx->get_inner()->pix_fmt = ictx.video_ctx->get_inner()->pix_fmt;

            octx.video_ctx->open(codec);
            octx.video_ctx->paste_params_to(o_stream->codecpar);
        }
    }

    octx.fmt_ctx.open_output(octx.filepath, AVIO_FLAG_WRITE);
    octx.fmt_ctx.write_header();

    Packet pkt;
    Frame frame;

    std::vector<AVStream*> ostreams = octx.fmt_ctx.streams();
    while (ictx.fmt_ctx.read_raw_frame(pkt) >= 0) {
        int stream_index = pkt.stream_index();
        if (stream_index == ictx.astream_idx) {
            AVStream* is = istreams[ictx.astream_idx];
            AVStream* os = ostreams[octx.astream_idx];
            pkt.rescale_ts(is->time_base, os->time_base);
            pkt.set_stream_index(octx.astream_idx);

            octx.fmt_ctx.write_frame_interleaved(pkt);
        } else if (stream_index == ictx.vstream_idx) {
            AVStream* is = istreams[ictx.vstream_idx];
            AVStream* os = ostreams[octx.vstream_idx];

            r = ictx.video_ctx->send_packet(pkt);
            handle_transcode_error(r < 0, "Could not send packet to the decoder");

            while ((r = ictx.video_ctx->receive_frame(frame)) >= 0) {
                r = octx.video_ctx->send_frame(frame);
                handle_transcode_error(r < 0, "Could not send frame to the encoder");

                while ((r = octx.video_ctx->receive_packet(pkt)) >= 0) {
                    pkt.rescale_ts(is->time_base, os->time_base);
                    pkt.set_stream_index(octx.vstream_idx);

                    octx.fmt_ctx.write_frame_interleaved(pkt);
                }
                frame.unref();
            }
        }   
    }

    // flush decoder
    r = ictx.audio_ctx->send_packet_flush();
    handle_transcode_error(r < 0, "Could not send flush packet to the audio decoder");
    while ((r = ictx.audio_ctx->receive_frame(frame)) >= 0) {
        AVStream* is = istreams[ictx.astream_idx];
        AVStream* os = ostreams[octx.astream_idx];
        pkt.rescale_ts(is->time_base, os->time_base);
        pkt.set_stream_index(octx.astream_idx);

        octx.fmt_ctx.write_frame_interleaved(pkt);
    }

    AVStream* is = istreams[ictx.vstream_idx];
    AVStream* os = ostreams[octx.vstream_idx];
    r = ictx.video_ctx->send_packet_flush();
    handle_transcode_error(r < 0, "Could not send flush packet to the video decoder")
    while ((r = ictx.video_ctx->receive_frame(frame)) >= 0) {
        r = octx.video_ctx->send_frame(frame);
        handle_transcode_error(r < 0, "Could not send frame to the encoder");

        while ((r = octx.video_ctx->receive_packet(pkt)) >= 0) {
            pkt.rescale_ts(is->time_base, os->time_base);
            pkt.set_stream_index(octx.vstream_idx);

            octx.fmt_ctx.write_frame_interleaved(pkt);
        }
        frame.unref();
    }

    // flush encoder
    r = octx.video_ctx->send_frame_flush();
    handle_transcode_error(r < 0, "Could not send flush frame to the encoder");
    while ((r = octx.video_ctx->receive_packet(pkt)) >= 0) {
        pkt.rescale_ts(is->time_base, os->time_base);
        pkt.set_stream_index(octx.vstream_idx);

        octx.fmt_ctx.write_frame_interleaved(pkt);
    }


    octx.fmt_ctx.write_trailer();
}
