

#include <stdexcept>
extern "C" {
    #include <libavcodec/packet.h>
    #include <libavutil/frame.h>
    #include <libavformat/avformat.h>
    #include <libavcodec/codec_par.h>
}
#include "formatcontext.hpp"
#include "stream.hpp"
#include "packet.hpp"

constexpr const char* INPUT = "edit.mp4";
constexpr const char* OUTPUT = "tmuxed_edit.ts";

int main() {
    InputFormatContext input_ctx;
    OutputFormatContext output_ctx;

    input_ctx.open_input(INPUT);
    input_ctx.find_best_stream_info();

    output_ctx.alloc_output(OUTPUT);

    std::vector<AVStream*> i_streams = input_ctx.streams();
    for (auto i = 0; i < i_streams.size(); ++i) {
        AVStream* in_stream = i_streams[i];
        StreamT out_stream = output_ctx.new_stream();
        if (!out_stream) {
            throw std::runtime_error("Could not create a fucking stream");
        }
        int ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
        if (ret < 0) {
            throw std::runtime_error("Could not copy parameters");
        }
        out_stream->codecpar->codec_tag = 0;
    }

    av_dump_format(output_ctx.get_inner(), 0, OUTPUT, true);

    output_ctx.open_output(OUTPUT, AVIO_FLAG_WRITE);
    output_ctx.write_header();

    auto o_streams = output_ctx.streams();
    Packet pkt;
    while (true) {
        int ret = input_ctx.read_raw_frame(pkt);
        if (ret < 0) {
            break;
        }

        int cur_idx = pkt.stream_index();
        auto* i_stream = i_streams[cur_idx];
        auto* o_stream = o_streams[cur_idx];

        pkt.rescale_ts(i_stream->time_base, o_stream->time_base);

        output_ctx.write_frame_interleaved(pkt);
    }

    output_ctx.write_trailer();
    return 0;
}