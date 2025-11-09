#pragma once
#include "codeccontext.hpp"
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <string_view>

void set_audio_codec_params(const AVCodec* codec, CodecContext& occtx, CodecContext& icctx, std::string_view encoder);
