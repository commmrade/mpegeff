#include <cstdio>
#include <iostream>
#include <stdexcept>
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


struct Context {
    AVFormatContext* fmt_ctx{nullptr};

    std::string filepath{};

    int astream_idx{};
    int vstream_idx{};

    AVCodecContext* audio_ctx{nullptr};
    AVCodecContext* video_ctx{nullptr};

    AVStream* audio_stream{nullptr};
    AVStream* video_stream{nullptr};

    const AVCodec* audio_codec{nullptr};
    const AVCodec* video_codec{nullptr};
};

#define handleError(cond, msg) if (cond) { throw std::runtime_error(msg); }

void transcode(std::string_view input, std::string_view output, Context& ictx, Context& octx) {
    ictx.fmt_ctx = avformat_alloc_context();
    handleError(!ictx.fmt_ctx, "Alloc fmt ctx");

    int r = avformat_open_input(&ictx.fmt_ctx, ictx.filepath.c_str(), NULL, NULL);
    handleError(r < 0, "Open input");

    r = avformat_find_stream_info(ictx.fmt_ctx, NULL);
    handleError(r < 0, "Find stream info");

    int n_streams = ictx.fmt_ctx->nb_streams;
    for (auto i = 0; i < n_streams; ++i) {
        AVCodecParameters* cparams = ictx.fmt_ctx->streams[i]->codecpar;
        const AVCodec* codec = avcodec_find_decoder(cparams->codec_id);
        handleError(!codec, "Find decoder")

        if (cparams->codec_type == AVMEDIA_TYPE_AUDIO) {
            ictx.astream_idx = i;
            ictx.audio_codec = codec;

            ictx.audio_ctx = avcodec_alloc_context3(codec);
            handleError(!ictx.audio_ctx, "Alloc context3");
            r = avcodec_parameters_to_context(ictx.audio_ctx, cparams);
            r = avcodec_open2(ictx.audio_ctx, codec, NULL);
            handleError(r < 0, "Open codec");
        } else if (cparams->codec_type == AVMEDIA_TYPE_VIDEO) {
            ictx.vstream_idx = i;
            ictx.video_codec = codec;

            ictx.video_ctx = avcodec_alloc_context3(codec);
            handleError(!ictx.video_ctx, "Alloc video ctx");
            avcodec_parameters_to_context(ictx.video_ctx, cparams);
            r = avcodec_open2(ictx.video_ctx, codec, NULL);
            handleError(r < 0, "Open v codec");
        }
    }


    r = avformat_alloc_output_context2(&octx.fmt_ctx, NULL, NULL, octx.filepath.c_str());
    handleError(r < 0, "Alloc output ctx");

    // Setup encoder
    for (auto i = 0; i < n_streams; ++i) {
        AVStream* i_stream = ictx.fmt_ctx->streams[i];
        int media_type = i_stream->codecpar->codec_type;
        if (media_type == AVMEDIA_TYPE_AUDIO) {
            // Mux audio
            
            AVStream* o_stream = avformat_new_stream(octx.fmt_ctx, NULL);
            handleError(!o_stream, "New stram audio");
            octx.audio_stream = o_stream;
            octx.astream_idx = i;
            avcodec_parameters_copy(o_stream->codecpar, i_stream->codecpar);
        } else if (media_type == AVMEDIA_TYPE_VIDEO) {
            octx.vstream_idx = i;
            AVStream* o_stream = avformat_new_stream(octx.fmt_ctx, NULL);
            handleError(!o_stream, "New stream video");
            octx.video_stream = o_stream;
            
            const AVCodec* codec = avcodec_find_encoder_by_name("libx265");
            handleError(!codec, "Find encoder");
            octx.video_codec = codec;

            AVCodecContext* video_ctx = avcodec_alloc_context3(codec);
            handleError(!video_ctx, "Could not alloc audio ctx");


            octx.video_ctx = video_ctx;
            
            octx.video_ctx->bit_rate = ictx.video_ctx->bit_rate;
            octx.video_ctx->time_base = av_inv_q(ictx.video_ctx->framerate);
            octx.video_ctx->height = ictx.video_ctx->height;
            octx.video_ctx->width = ictx.video_ctx->width;
            octx.video_ctx->pix_fmt = ictx.video_ctx->pix_fmt;
            // octx.video_ctx->rc_buffer_size = ictx.video_ctx->rc_buffer_size;
            // octx.video_ctx->rc_max_rate = 2 * 1000 * 1000;
            // octx.video_ctx->rc_min_rate = 2.5 * 1000 * 1000;

            r = avcodec_open2(video_ctx, codec, NULL);
            handleError(r < 0, "Open codec encoder");
            r = avcodec_parameters_from_context(o_stream->codecpar, video_ctx);
            handleError(r < 0, "Copy parameters");
        }
    }

    r = avio_open(&octx.fmt_ctx->pb, octx.filepath.c_str(), AVIO_FLAG_WRITE);
    handleError(r < 0, "Open avio")

    r = avformat_write_header(octx.fmt_ctx, NULL);
    handleError(r < 0, "Write header");

    av_dump_format(octx.fmt_ctx, 0, 0, true);

    AVPacket* pkt = av_packet_alloc();
    handleError(!pkt, "Could not pkt alloc");
    AVFrame* frame = av_frame_alloc();
    handleError(!frame, "Could not alloc frae");


    while (av_read_frame(ictx.fmt_ctx, pkt) >= 0) {
        int stream_index = pkt->stream_index;
        if (stream_index == ictx.astream_idx) {
            AVStream* is = ictx.fmt_ctx->streams[ictx.astream_idx];
            AVStream* os = octx.fmt_ctx->streams[octx.astream_idx];
            av_packet_rescale_ts(pkt, is->time_base, os->time_base);
            pkt->pos = -1;
            pkt->stream_index = octx.astream_idx;

            av_interleaved_write_frame(octx.fmt_ctx, pkt);
        } else if (stream_index == ictx.vstream_idx) {
            AVStream* is = ictx.fmt_ctx->streams[ictx.vstream_idx];
            AVStream* os = octx.fmt_ctx->streams[octx.vstream_idx];

            r = avcodec_send_packet(ictx.video_ctx, pkt);
            handleError(r < 0, "Decode packet");

            while ((r = avcodec_receive_frame(ictx.video_ctx, frame)) >= 0) {

                r = avcodec_send_frame(octx.video_ctx, frame);
                handleError(r < 0, "Encode frame");

                while ((r = avcodec_receive_packet(octx.video_ctx, pkt)) >= 0) {
                    av_packet_rescale_ts(pkt, is->time_base, os->time_base);
                    pkt->pos = -1;
                    pkt->stream_index = octx.vstream_idx;

                    av_interleaved_write_frame(octx.fmt_ctx, pkt);
                }
            }
        }   
        // r = avcodec_send_packet(ictx.c, pkt);

    }

    r = av_write_trailer(octx.fmt_ctx);
}


int main() {
    // remux(INPUT, OUTPUT);
    Context i{};
    i.filepath = "edit.mp4";
    Context o{};
    o.filepath = "svo.mp4";
    transcode("edit.mp4", "svo.mp4", i, o);
    return 0;
}