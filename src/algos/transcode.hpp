#pragma once
#include "formatcontext.hpp"
#include "codeccontext.hpp"
#include <cstdio>
#include <format>
#include <iostream>
#include <libavutil/mathematics.h>
extern "C" {
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
            // StreamT o_stream = octx.fmt_ctx.new_stream();
            // octx.audio_stream = o_stream;
            // octx.astream_idx = i;
            // avcodec_parameters_copy(o_stream->codecpar, i_stream->codecpar);


            StreamT o_stream = octx.fmt_ctx.new_stream();
            octx.audio_stream = o_stream;
            octx.astream_idx = i;

            const char* encoder_name = "aac";
            const AVCodec* codec = avcodec_find_encoder_by_name(encoder_name);
            handle_transcode_error(!codec, std::format("Could not find encoder by name {}", encoder_name));

            octx.audio_codec = codec;

            octx.audio_ctx = std::make_unique<CodecContext>(codec);
            octx.audio_ctx->get_inner()->bit_rate = ictx.audio_ctx->get_inner()->bit_rate;
            octx.audio_ctx->get_inner()->time_base = AVRational{1, octx.audio_ctx->get_inner()->sample_rate};
            octx.audio_ctx->get_inner()->sample_rate = ictx.audio_ctx->get_inner()->sample_rate;

            const AVSampleFormat* sf = NULL;
            const AVChannelLayout* cl = NULL;

            avcodec_get_supported_config(octx.audio_ctx->get_inner(), codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, (const void**)&sf, NULL);
            avcodec_get_supported_config(octx.audio_ctx->get_inner(), codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, (const void**)&cl, NULL);

            octx.audio_ctx->get_inner()->sample_fmt = *sf;
            if (cl) {
                av_channel_layout_copy(&octx.audio_ctx->get_inner()->ch_layout, cl);
            } else {
                av_channel_layout_default(&octx.audio_ctx->get_inner()->ch_layout, 2);
            }


            octx.audio_ctx->open(codec);
            octx.audio_ctx->paste_params_to(o_stream->codecpar);
        } else if (media_type == AVMEDIA_TYPE_VIDEO) {
            octx.vstream_idx = i;
            StreamT o_stream = octx.fmt_ctx.new_stream();
            octx.video_stream = o_stream;

            // const char* encoder_name = "libaom-av1"; // TODO: CHange
            const char* encoder_name = "libsvtav1";
            const AVCodec* codec = avcodec_find_encoder_by_name(encoder_name);
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

    av_dump_format(octx.fmt_ctx.get_inner(), 0, octx.filepath.c_str(), true);

    Packet pkt;
    Frame frame;

    std::vector<AVStream*> ostreams = octx.fmt_ctx.streams();


    SwrContext* swr_ctx = swr_alloc();
    handle_transcode_error(!swr_ctx, "Could not alloc swr ctx");

    swr_alloc_set_opts2(
        &swr_ctx,
        &octx.audio_ctx->get_inner()->ch_layout,        // out channel layout
        octx.audio_ctx->get_inner()->sample_fmt,        // out sample_fmt
        octx.audio_ctx->get_inner()->sample_rate,       // out sample_rate
        &ictx.audio_ctx->get_inner()->ch_layout,        // in channel layout
        ictx.audio_ctx->get_inner()->sample_fmt,        // in sample_fmt
        ictx.audio_ctx->get_inner()->sample_rate,       // in sample_rate
        0, nullptr
    );


    r = swr_init(swr_ctx);
    handle_transcode_error(r < 0, "Could njot init swr context");

    AVAudioFifo* fifo = av_audio_fifo_alloc(octx.audio_ctx->get_inner()->sample_fmt, octx.audio_ctx->get_inner()->ch_layout.nb_channels, 1);

    int64_t pts = 0;
    while (ictx.fmt_ctx.read_raw_frame(pkt) >= 0) {
        int stream_index = pkt.stream_index();
        if (stream_index == ictx.astream_idx) {
            AVStream* is = istreams[ictx.astream_idx];
            AVStream* os = ostreams[octx.astream_idx];
            // pkt.rescale_ts(is->time_base, os->time_base);
            // pkt.set_stream_index(octx.astream_idx);
            // octx.fmt_ctx.write_frame_interleaved(pkt);

            r = ictx.audio_ctx->send_packet(pkt);
            handle_transcode_error(r < 0, "Could not send packet to the decoder");

            while ((r = ictx.audio_ctx->receive_frame(frame)) >= 0) {
                // printf("Samples nb: %d, frame size: %d\n", frame.get_inner()->nb_samples, octx.audio_ctx->get_inner()->frame_size);

                uint8_t** converted_samples_buf = nullptr;
                r = av_samples_alloc_array_and_samples(
                    &converted_samples_buf,
                    NULL,
                    octx.audio_ctx->get_inner()->ch_layout.nb_channels,
                    frame.get_inner()->nb_samples,
                    octx.audio_ctx->get_inner()->sample_fmt,
                    0);
                handle_transcode_error(r < 0, "Could not alloc converted samples buf");

                int frame_size = octx.audio_ctx->get_inner()->frame_size;
                int nb_samples = frame.get_inner()->nb_samples;

                nb_samples = swr_convert(swr_ctx, converted_samples_buf, nb_samples, frame.get_inner()->extended_data, nb_samples);
                // printf("Swr convert: %d\n", r);
                handle_transcode_error(nb_samples < 0, "Could not convert");

                r = av_audio_fifo_realloc(fifo, av_audio_fifo_size(fifo) + nb_samples);
                handle_transcode_error(r < 0, "COuld not realloc");

                r = av_audio_fifo_write(fifo, (void**)converted_samples_buf, nb_samples);
                handle_transcode_error(r < 0, "COuld not write to fifo");

                while (av_audio_fifo_size(fifo) >= frame_size) {
                    Frame frame;
                    frame.get_inner()->nb_samples = frame_size;
                    av_channel_layout_copy(&frame.get_inner()->ch_layout, &octx.audio_ctx->get_inner()->ch_layout);
                    frame.get_inner()->format = octx.audio_ctx->get_inner()->sample_fmt;
                    frame.get_inner()->sample_rate = octx.audio_ctx->get_inner()->sample_rate;

                    r = av_frame_get_buffer(frame.get_inner(), 0);
                    handle_transcode_error(r < 0, "Could not get buffer in while");

                    av_audio_fifo_read(fifo, (void**)frame.get_inner()->data, frame_size);

                    // av_rescale_q(int64_t a, AVRational bq, AVRational cq)
                    // frame.get_inner()->pts = av_rescale_q(frame.get_inner()->best_effort_timestamp, {1, octx.audio_ctx->get_inner()->sample_rate}, os->time_base);

                    frame.get_inner()->pts = pts;
                    pts += frame_size;

                    r = octx.audio_ctx->send_frame(frame);
                    // printf("Frame size: %d\n", frame_size);
                    handle_transcode_error(r < 0, "Could not send frame to the encoder in loop");
                    while ((r = octx.audio_ctx->receive_packet(pkt)) >= 0) {
                        pkt.rescale_ts(is->time_base, os->time_base);
                        pkt.set_stream_index(octx.astream_idx);


                        octx.fmt_ctx.write_frame_interleaved(pkt);
                    }
                    av_packet_unref(pkt.get_inner());
                }

                // std::printf("Frame size: %d, fifo size: %d\n", frame_size, av_audio_fifo_size(fifo));
                // int size_left = std::min(frame_size, av_audio_fifo_size(fifo));
                // if (size_left == 0) continue;
                // std::printf("Size left: %d\n", size_left);
                // Frame frame;
                // frame.get_inner()->nb_samples = size_left;
                // av_channel_layout_copy(&frame.get_inner()->ch_layout, &octx.audio_ctx->get_inner()->ch_layout);
                // frame.get_inner()->format = octx.audio_ctx->get_inner()->sample_fmt;
                // frame.get_inner()->sample_rate = octx.audio_ctx->get_inner()->sample_rate;

                // r = av_frame_get_buffer(frame.get_inner(), 0);
                // handle_transcode_error(r < 0, "Could not get buffer outside");

                // r = av_audio_fifo_read(fifo, (void**)frame.get_inner()->data, size_left);
                // handle_transcode_error(r < 0, "Could not read fifo");

                // r = octx.audio_ctx->send_frame(frame);
                // handle_transcode_error(r < 0, "Could not send frame to the encoder");
                // while ((r = octx.audio_ctx->receive_packet(pkt)) >= 0) {
                //     pkt.rescale_ts(is->time_base, os->time_base);
                //     pkt.set_stream_index(octx.astream_idx);


                //     octx.fmt_ctx.write_frame_interleaved(pkt);
                // }
                // av_packet_unref(pkt.get_inner());
            }
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
                av_packet_unref(pkt.get_inner());
                frame.unref();
            }
        }
        av_packet_unref(pkt.get_inner());
    }

    // flush decoder
    // r = ictx.audio_ctx->send_packet_flush();
    // handle_transcode_error(r < 0, "Could not send flush packet to the audio decoder");
    // while ((r = ictx.audio_ctx->receive_frame(frame)) >= 0) {
    //     AVStream* is = istreams[ictx.astream_idx];
    //     AVStream* os = ostreams[octx.astream_idx];
    //     pkt.rescale_ts(is->time_base, os->time_base);
    //     pkt.set_stream_index(octx.astream_idx);

    //     octx.fmt_ctx.write_frame_interleaved(pkt);
    // }

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
    // TODO: Flush audio encoder


    octx.fmt_ctx.write_trailer();
}
