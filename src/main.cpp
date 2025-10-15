#include <cstdio>
#include <iostream>
#include <stdexcept>
#include <format>
extern "C" {
    #include <libavformat/avio.h>
    #include <libavutil/rational.h>
    #include <libavcodec/codec.h>
    #include <libavutil/avutil.h>
    #include <libavcodec/avcodec.h>
    #include <libavcodec/packet.h>
    #include <libavutil/frame.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/codec_par.h>
}
#include "formatcontext.hpp"
#include "stream.hpp"
#include "packet.hpp"
#include "frame.hpp"
#include "codeccontext.hpp"
#include "exceptions.hpp"


constexpr const char* INPUT = "edit.mp4";
constexpr const char* OUTPUT = "tmuxed_edit.mov";

void remux(std::string_view input, std::string_view output) {
    InputFormatContext ictx{};
    OutputFormatContext octx;

    ictx.open_input(INPUT);
    ictx.find_best_stream_info();

    octx.alloc_output(output);

    std::vector<AVStream*> streams = ictx.streams();
    std::size_t streams_sz = streams.size();
    std::cout << "Streams " << streams.size() << "\n";
    for (auto i = 0; i < streams_sz; ++i) {
        AVStream* istream = streams[i];
        StreamT ostream = octx.new_stream();
        avcodec_parameters_copy(ostream->codecpar, istream->codecpar);
        ostream->codecpar->codec_tag = 0;
    }

    octx.open_output(output, AVIO_FLAG_WRITE);
    octx.write_header();

    Packet pkt;
    std::vector<AVStream*> ostreams = octx.streams();
    while (true) {
        int res = ictx.read_raw_frame(pkt);
        if (res < 0) {
            break;
        }

        int stream_index = pkt.stream_index();
        AVStream* istream = streams[stream_index];
        AVStream* ostream = ostreams[stream_index];

        pkt.rescale_ts(istream->time_base, ostream->time_base);
        octx.write_frame_interleaved(pkt);
    }
    
    octx.write_trailer();
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


int main() {
    // remux(INPUT, OUTPUT);
    IContext i{};
    i.filepath = "edit.mp4";
    OContext o{};
    o.filepath = "svo.mp4";
    transcode("edit.mp4", "svo.mp4", i, o);
    return 0;
}