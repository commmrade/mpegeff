#include "packet.hpp"

Packet::Packet() { m_pkt = av_packet_alloc(); }
Packet::~Packet() { av_packet_free(&m_pkt); }
void Packet::rescale_ts(AVRational src_tb, AVRational dst_tb) {
    av_packet_rescale_ts(m_pkt, src_tb, dst_tb);
    m_pkt->pos = -1;
}
int Packet::stream_index() const { return m_pkt->stream_index; }
void Packet::set_stream_index(int idx) { m_pkt->stream_index = idx; }
