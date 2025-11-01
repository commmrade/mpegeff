#pragma once

#include <stdexcept>
#include <string_view>

struct format_context_error : public std::runtime_error {
    format_context_error(std::string_view msg) : std::runtime_error(std::string{msg}) {}
};

struct cctx_error : public std::runtime_error {
    cctx_error(std::string_view msg) : std::runtime_error(std::string{msg}) {}
};

struct transcoding_error : public std::runtime_error {
    transcoding_error(std::string_view msg) : std::runtime_error(std::string{msg}) {}
};

struct audio_fifo_error : public std::runtime_error {
    audio_fifo_error(std::string_view msg) : std::runtime_error(std::string{msg}) {}
};
