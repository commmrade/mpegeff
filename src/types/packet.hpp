#pragma once

extern "C" {
    #include <libavutil/rational.h>
    #include <libavcodec/packet.h>
}

class Packet {
    AVPacket* m_pkt{nullptr};
public:
    Packet(AVPacket* pkt) : m_pkt(pkt)
    {}
    Packet();
    ~Packet();

    void rescale_ts(AVRational src_tb, AVRational dst_tb);

    int stream_index() const;
    void set_stream_index(int idx);

    AVPacket* get_inner() {
        return m_pkt;
    }
    const AVPacket* get_inner() const {
        return m_pkt;
    }
};
