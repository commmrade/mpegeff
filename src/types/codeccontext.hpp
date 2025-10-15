#pragma once
#include <new>
#include "exceptions.hpp"
#include "frame.hpp"
#include "packet.hpp"
extern "C" {
    #include <libavcodec/avcodec.h>
}

class CodecContext {
    AVCodecContext* m_cctx{nullptr};
public:
    CodecContext(const AVCodec* codec) {
        m_cctx = avcodec_alloc_context3(codec);
        if (!m_cctx) {
            throw std::bad_alloc{};
        }
    }
    ~CodecContext() {
        avcodec_free_context(&m_cctx);
    }

    int send_packet(const Packet& pkt) {
        return avcodec_send_packet(m_cctx, pkt.get_inner());
    }
    int send_packet_flush() {
        return avcodec_send_packet(m_cctx, NULL);
    }
    int receive_frame(Frame& frame) {
        return avcodec_receive_frame(m_cctx, frame.get_inner());
    }

    

    int send_frame(const Frame& frame) {
        return avcodec_send_frame(m_cctx, frame.get_inner());
    }
    int send_frame_flush() {
        return avcodec_send_frame(m_cctx, NULL);
    }
    int receive_packet(Packet& pkt) {
        return avcodec_receive_packet(m_cctx, pkt.get_inner());
    }

    void open(const AVCodec* codec, AVDictionary** dict = nullptr) {
        int r = avcodec_open2(m_cctx, codec, dict);
        if (r < 0) {
            throw cctx_error{"Could not open codec"};
        }
    }

    void copy_params_from(AVCodecParameters* params) {
        int r = avcodec_parameters_to_context(m_cctx, params);
        if (r < 0) {
            throw cctx_error{"Could not copy parameters"};
        }
    }

    void paste_params_to(AVCodecParameters* params) {
        avcodec_parameters_from_context(params, m_cctx);
    }
    
    AVCodecContext* get_inner() {
        return m_cctx;
    }
    const AVCodecContext* get_inner() const {
        return m_cctx;
    }
};