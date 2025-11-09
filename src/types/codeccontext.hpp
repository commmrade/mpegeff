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
  CodecContext(const AVCodec *codec);
  ~CodecContext() { avcodec_free_context(&m_cctx); }

  int send_packet(const Packet &pkt);
  int send_packet_flush();
  int receive_frame(Frame &frame);

  int send_frame(const Frame &frame);
  int send_frame_flush();
  int receive_packet(Packet &pkt);

  void open(const AVCodec *codec, AVDictionary **dict = nullptr);

  void copy_params_from(AVCodecParameters *params);
  void paste_params_to(AVCodecParameters *params);

  AVCodecContext *get_inner() { return m_cctx; }
  const AVCodecContext *get_inner() const { return m_cctx; }
};
