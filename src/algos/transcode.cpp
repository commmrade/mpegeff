#include "transcode.hpp"
#include <format>
#include "../utils.hpp"

void transcode(IContext& ictx, OContext& octx, std::string_view codec_audio, std::string_view codec_video) {
    ictx.fmt_ctx.open_input(ictx.filepath);
    ictx.fmt_ctx.find_best_stream_info();

    int r = 0;
    std::vector<AVStream*> istreams = ictx.fmt_ctx.streams();
    for (std::size_t i = 0; i < istreams.size(); ++i) {
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
    for (std::size_t i = 0; i < istreams.size(); ++i) {
        AVStream* i_stream = istreams[i];
        int media_type = i_stream->codecpar->codec_type;
        if (media_type == AVMEDIA_TYPE_AUDIO) {

            if (ictx.mux_audio) {
                StreamT o_stream = octx.fmt_ctx.new_stream();
                octx.audio_stream = o_stream;
                octx.astream_idx = i;
                avcodec_parameters_copy(o_stream->codecpar, i_stream->codecpar);
            } else {
                StreamT o_stream = octx.fmt_ctx.new_stream();
                octx.audio_stream = o_stream;
                octx.astream_idx = i;

                const AVCodec* codec = avcodec_find_encoder_by_name(codec_audio.data());
                handle_transcode_error(!codec, std::format("Could not find encoder by name {}", codec_audio.data()));

                octx.audio_codec = codec;

                // TODO: HElper function to setup this params based on codec, since auto stuff sucks
                octx.audio_ctx = std::make_unique<CodecContext>(codec);

                set_audio_codec_params(codec, *octx.audio_ctx, *ictx.audio_ctx, codec_audio);

                octx.audio_ctx->open(codec);
                octx.audio_ctx->paste_params_to(o_stream->codecpar);
            }
        } else if (media_type == AVMEDIA_TYPE_VIDEO) {

            if (ictx.mux_video) {
                StreamT video_stream = octx.fmt_ctx.new_stream();
                octx.video_stream = video_stream;
                octx.vstream_idx = i;
                avcodec_parameters_copy(video_stream->codecpar, i_stream->codecpar);
            } else {
                octx.vstream_idx = i;
                StreamT o_stream = octx.fmt_ctx.new_stream();
                octx.video_stream = o_stream;

                const AVCodec* codec = avcodec_find_encoder_by_name(codec_video.data());
                handle_transcode_error(!codec, std::format("Could not find encoder by name {}", codec_video.data()));

                octx.video_codec = codec;

                octx.video_ctx = std::make_unique<CodecContext>(codec);
                octx.video_ctx->get_inner()->bit_rate = ictx.video_ctx->get_inner()->bit_rate;

                AVRational fr = ictx.video_ctx->get_inner()->framerate; // Try to guess framerate from input codec context
                if (fr.num == 0 || fr.den == 0) {
                    fr = i_stream->avg_frame_rate; // Input codec context might now containe framerate data, so fall back to stream framerate
                }
                if (fr.num == 0 || fr.den == 0) { // If stream is fucked up and does not contain framerate data, fall back to hard coded framerate
                    fr = av_make_q(24, 1);
                }

                octx.video_ctx->get_inner()->framerate = fr;
                octx.video_ctx->get_inner()->time_base = av_inv_q(fr);

                octx.video_ctx->get_inner()->height = ictx.video_ctx->get_inner()->height;
                octx.video_ctx->get_inner()->width = ictx.video_ctx->get_inner()->width;
                octx.video_ctx->get_inner()->pix_fmt = ictx.video_ctx->get_inner()->pix_fmt;

                octx.video_ctx->open(codec);
                octx.video_ctx->paste_params_to(o_stream->codecpar);
            }

        }
    }

    octx.fmt_ctx.open_output(octx.filepath, AVIO_FLAG_WRITE);
    octx.fmt_ctx.write_header();

    av_dump_format(octx.fmt_ctx.get_inner(), 0, octx.filepath.c_str(), true);

    Packet pkt;
    Frame frame;

    std::vector<AVStream*> ostreams = octx.fmt_ctx.streams();

    // TODO: Fix if mux
    std::unique_ptr<SwrCtx> swr_ctx;
    std::unique_ptr<AudioFifo> fifo;
    if (!ictx.mux_audio) {
        swr_ctx = std::make_unique<SwrCtx>(&octx.audio_ctx->get_inner()->ch_layout,
            octx.audio_ctx->get_inner()->sample_fmt,
            octx.audio_ctx->get_inner()->sample_rate,
            &ictx.audio_ctx->get_inner()->ch_layout,
            ictx.audio_ctx->get_inner()->sample_fmt,
            ictx.audio_ctx->get_inner()->sample_rate
        );
        fifo = std::make_unique<AudioFifo>(
            octx.audio_ctx->get_inner()->sample_fmt,
            octx.audio_ctx->get_inner()->ch_layout.nb_channels,
            1
        );
        swr_ctx->init();
    }

    int64_t pts = 0;
    while (ictx.fmt_ctx.read_raw_frame(pkt) >= 0) {
        int stream_index = pkt.stream_index();
        if (stream_index == ictx.astream_idx) {
            AVStream* is = istreams[ictx.astream_idx];
            AVStream* os = ostreams[octx.astream_idx];

            if (ictx.mux_audio) {
                pkt.rescale_ts(is->time_base, os->time_base);
                pkt.set_stream_index(octx.astream_idx);
                octx.fmt_ctx.write_frame_interleaved(pkt);
            } else {
                r = ictx.audio_ctx->send_packet(pkt);
                handle_transcode_error(r < 0, "Could not send packet to the decoder");

                while ((r = ictx.audio_ctx->receive_frame(frame)) >= 0) {
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

                    nb_samples = swr_ctx->convert(converted_samples_buf, nb_samples, frame.get_inner()->extended_data, nb_samples);
                    handle_transcode_error(nb_samples < 0, "Could not convert");

                    fifo->resize(fifo->remaining_size() + nb_samples);
                    fifo->write((void**)converted_samples_buf, nb_samples);

                    while (fifo->remaining_size() >= frame_size) {
                        Frame frame;
                        frame.get_inner()->nb_samples = frame_size;
                        av_channel_layout_copy(&frame.get_inner()->ch_layout, &octx.audio_ctx->get_inner()->ch_layout);
                        frame.get_inner()->format = octx.audio_ctx->get_inner()->sample_fmt;
                        frame.get_inner()->sample_rate = octx.audio_ctx->get_inner()->sample_rate;
                        frame.make_buffer();

                        fifo->read((void**)frame.get_inner()->data, frame_size);

                        frame.get_inner()->pts = pts;
                        pts += frame_size;

                        r = octx.audio_ctx->send_frame(frame);
                        handle_transcode_error(r < 0, "Could not send frame to the encoder in loop");
                        while ((r = octx.audio_ctx->receive_packet(pkt)) >= 0) {
                            pkt.rescale_ts(is->time_base, os->time_base);
                            pkt.set_stream_index(octx.astream_idx);


                            octx.fmt_ctx.write_frame_interleaved(pkt);
                        }
                        av_packet_unref(pkt.get_inner());
                    }

                    av_freep(&converted_samples_buf[0]);
                    av_freep(&converted_samples_buf);
                }
            }
        } else if (stream_index == ictx.vstream_idx) {
            AVStream* is = istreams[ictx.vstream_idx];
            AVStream* os = ostreams[octx.vstream_idx];

            if (ictx.mux_video) {
                pkt.rescale_ts(is->time_base, os->time_base);
                pkt.set_stream_index(octx.vstream_idx);
                octx.fmt_ctx.write_frame_interleaved(pkt);
            } else {
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
        }
        av_packet_unref(pkt.get_inner());
    }

    // TODO: Fix flash for transmux flag
    if (!ictx.mux_audio || !ictx.mux_video) {
        flush(ictx, octx, std::move(swr_ctx), std::move(fifo), pts);
    }

    octx.fmt_ctx.write_trailer();
}


void flush(IContext& ictx, OContext& octx, std::unique_ptr<SwrCtx> swr_ctx, std::unique_ptr<AudioFifo> fifo, int64_t pts) {
    std::vector<AVStream*> istreams = ictx.fmt_ctx.streams();
    std::vector<AVStream*> ostreams = octx.fmt_ctx.streams();
    // TODO: FIX obu_reserved_1bit out of range: 1, but must be in [0,0]
    Frame frame;
    Packet pkt;
    int r;

    if (!ictx.mux_audio) {
        r = ictx.audio_ctx->send_packet_flush();
        handle_transcode_error(r < 0, "Could not send flush packet to the audio decoder");
        while ((r = ictx.audio_ctx->receive_frame(frame)) >= 0) {
            AVStream* is = istreams[ictx.astream_idx];
            AVStream* os = ostreams[octx.astream_idx];

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

            nb_samples = swr_ctx->convert(converted_samples_buf, nb_samples, frame.get_inner()->extended_data, nb_samples);
            handle_transcode_error(nb_samples < 0, "Could not convert");

            fifo->resize(fifo->remaining_size() + nb_samples);
            fifo->write((void**)converted_samples_buf, nb_samples);

            while (fifo->remaining_size() >= frame_size) {
                Frame frame;
                frame.get_inner()->nb_samples = frame_size;
                av_channel_layout_copy(&frame.get_inner()->ch_layout, &octx.audio_ctx->get_inner()->ch_layout);
                frame.get_inner()->format = octx.audio_ctx->get_inner()->sample_fmt;
                frame.get_inner()->sample_rate = octx.audio_ctx->get_inner()->sample_rate;
                frame.make_buffer();

                fifo->read((void**)frame.get_inner()->data, frame_size);

                frame.get_inner()->pts = pts;
                r = octx.audio_ctx->send_frame(frame);
                handle_transcode_error(r < 0, "Could not send frame to the encoder in loop");
                while ((r = octx.audio_ctx->receive_packet(pkt)) >= 0) {
                    pkt.rescale_ts(is->time_base, os->time_base);
                    pkt.set_stream_index(octx.astream_idx);
                    octx.fmt_ctx.write_frame_interleaved(pkt);
                }
                av_packet_unref(pkt.get_inner());
            }

            delete[] converted_samples_buf;
        }
    }


    AVStream* is = istreams[ictx.vstream_idx];
    AVStream* os = ostreams[octx.vstream_idx];

    if (!ictx.mux_video) {
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
    }

    {
        if (!ictx.mux_audio) {
            int size_left = fifo->remaining_size();
            if (size_left) {
                Frame frame;
                frame.get_inner()->nb_samples = size_left;
                av_channel_layout_copy(&frame.get_inner()->ch_layout, &octx.audio_ctx->get_inner()->ch_layout);
                frame.get_inner()->format = octx.audio_ctx->get_inner()->sample_fmt;
                frame.get_inner()->sample_rate = octx.audio_ctx->get_inner()->sample_rate;
                frame.make_buffer();

                fifo->read((void**)frame.get_inner()->data, size_left);

                frame.get_inner()->pts = pts;
                r = octx.audio_ctx->send_frame(frame);
                handle_transcode_error(r < 0, "Could not send frame to the encoder in loop");
                while ((r = octx.audio_ctx->receive_packet(pkt)) >= 0) {
                    pkt.rescale_ts(is->time_base, os->time_base);
                    pkt.set_stream_index(octx.astream_idx);

                    octx.fmt_ctx.write_frame_interleaved(pkt);
                }
                av_packet_unref(pkt.get_inner());
            }
        }
    }


    // flush encoder
    if (!ictx.mux_video) {
        r = octx.video_ctx->send_frame_flush();
        handle_transcode_error(r < 0, "Could not send flush frame to the encoder");
        while ((r = octx.video_ctx->receive_packet(pkt)) >= 0) {
            pkt.rescale_ts(is->time_base, os->time_base);
            pkt.set_stream_index(octx.vstream_idx);

            octx.fmt_ctx.write_frame_interleaved(pkt);
        }
        av_packet_unref(pkt.get_inner());
    }

    if (!ictx.mux_audio) {
        r = octx.audio_ctx->send_frame_flush();
        handle_transcode_error(r < 0, "Could not send frame to the encoder in loop");
        while ((r = octx.audio_ctx->receive_packet(pkt)) >= 0) {
            pkt.rescale_ts(is->time_base, os->time_base);
            pkt.set_stream_index(octx.astream_idx);

            octx.fmt_ctx.write_frame_interleaved(pkt);
        }
        av_packet_unref(pkt.get_inner());

    }
}
