#pragma once

extern "C" {
    #include <libavutil/rational.h>
    #include <libavcodec/packet.h>
}

class Packet {
    AVPacket* m_pkt;
public:
    Packet(AVPacket* pkt) : m_pkt(pkt) 
    {}
    Packet() {
        m_pkt = av_packet_alloc();
    }
    ~Packet() {
        av_packet_free(&m_pkt);
    }

    void rescale_ts(AVRational src_tb, AVRational dst_tb) {
        av_packet_rescale_ts(m_pkt, src_tb, dst_tb);
        m_pkt->pos = -1;
    }


    int stream_index() const {
        return m_pkt->stream_index;
    }
    void set_stream_index(int idx) {
        m_pkt->stream_index = idx;
    }

    AVPacket* get_inner() {
        return m_pkt;
    }
    const AVPacket* get_inner() const {
        return m_pkt;
    }
};