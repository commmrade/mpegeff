#include <iostream>
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

int main() {
    remux(INPUT, OUTPUT);
    return 0;
}