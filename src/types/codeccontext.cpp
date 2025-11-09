#include "codeccontext.hpp"

CodecContext::CodecContext(const AVCodec *codec) {
    m_cctx = avcodec_alloc_context3(codec);
    if (!m_cctx) {
        throw std::bad_alloc{};
    }
}


void CodecContext::copy_params_from(AVCodecParameters *params) {
    int r = avcodec_parameters_to_context(m_cctx, params);
    if (r < 0) {
        throw cctx_error{"Could not copy parameters"};
    }
}
int CodecContext::send_packet(const Packet &pkt) {
    return avcodec_send_packet(m_cctx, pkt.get_inner());
}
int CodecContext::send_packet_flush() {
    return avcodec_send_packet(m_cctx, NULL);
}
int CodecContext::receive_frame(Frame &frame) {
    return avcodec_receive_frame(m_cctx, frame.get_inner());
}
int CodecContext::send_frame(const Frame &frame) {
    return avcodec_send_frame(m_cctx, frame.get_inner());
}
int CodecContext::send_frame_flush() {
    return avcodec_send_frame(m_cctx, NULL);
}
int CodecContext::receive_packet(Packet &pkt) {
    return avcodec_receive_packet(m_cctx, pkt.get_inner());
}
void CodecContext::open(const AVCodec *codec, AVDictionary **dict) {
    int r = avcodec_open2(m_cctx, codec, dict);
    if (r < 0) {
        throw cctx_error{"Could not open codec"};
    }
}
void CodecContext::paste_params_to(AVCodecParameters *params) {
    avcodec_parameters_from_context(params, m_cctx);
}
