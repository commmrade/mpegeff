#pragma once
#include "formatcontext.hpp"
#include <iostream>
void remux(std::string_view input, std::string_view output) {
    InputFormatContext ictx{};
    OutputFormatContext octx;

    ictx.open_input(input);
    ictx.find_best_stream_info();

    octx.alloc_output(output);

    std::vector<AVStream*> streams = ictx.streams();
    std::size_t streams_sz = streams.size();
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
