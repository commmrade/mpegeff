#pragma once
#include <libavformat/avio.h>
#include <libavutil/dict.h>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>
#include "exceptions.hpp"
#include "stream.hpp"
#include "packet.hpp"
extern "C" {
    #include <libavformat/avformat.h>
}


class FormatContextBase {
protected:
    AVFormatContext* m_ctx{nullptr};
public:
    FormatContextBase() {
        m_ctx = avformat_alloc_context();
        if (!m_ctx) throw std::bad_alloc{};
    }
    virtual ~FormatContextBase() { avformat_free_context(m_ctx); }

    unsigned int streams_count() const { return m_ctx->nb_streams; }
    std::vector<AVStream*> streams() {
        std::vector<AVStream*> res;
        int n = streams_count();
        res.reserve(n);
        for (auto i = 0; i < n; ++i) res.push_back(m_ctx->streams[i]);
        return res;
    }

    AVFormatContext* get_inner() { return m_ctx; }
    const AVFormatContext* get_inner() const { return m_ctx; }
};

class InputFormatContext : public FormatContextBase {
    bool m_is_open_input{false};
public:
    InputFormatContext() {
        m_ctx = avformat_alloc_context();
        if (!m_ctx) {
            throw std::bad_alloc{};
        }
    }
    ~InputFormatContext() override {
        free_context();
    }
    void free_context() {
        if (m_is_open_input) {
            close_input();
        }
    }

    // input output
    void open_input(const std::string_view url, const AVInputFormat* fmt = nullptr, AVDictionary** options = nullptr) {
        int ret = avformat_open_input(&m_ctx, url.data(), fmt, options);
        if (ret < 0) throw format_context_error{"Could not open input"};
        m_is_open_input = true;
    }
    void close_input() {
        avformat_close_input(&m_ctx);
        m_is_open_input = false;
    }

    int read_raw_frame(Packet& pkt) {
        return av_read_frame(m_ctx, pkt.get_inner());
    }
    std::optional<Packet> read_raw_frame() {
        Packet pkt;
        int r = av_read_frame(m_ctx, pkt.get_inner());
        if (r < 0) return {std::nullopt};
        return {std::move(pkt)};
    }

    void find_best_stream_info(AVDictionary** options = nullptr) {
        int ret = avformat_find_stream_info(m_ctx, options);
        if (ret < 0) throw format_context_error{"Could not find stream info"};
    }
};



class OutputFormatContext : public FormatContextBase {
    bool m_is_open_output{false};
public:
    OutputFormatContext() = default;
    ~OutputFormatContext() override {
        free_context();
    }
    void free_context() {
        if (m_is_open_output) {
            close_output();
        }
    }


    void alloc_output(const std::string_view filename, const std::string_view format_name = {}, const AVOutputFormat* oformat = nullptr) {
        if (m_ctx) {
            free_context();
        }
        int ret = avformat_alloc_output_context2(&m_ctx, oformat, format_name.data(), filename.data());
        if (ret < 0) throw format_context_error{"COuld not alloc output context"};
    }
    void open_output(const std::string_view url, int flags) {
        int r = avio_open(&m_ctx->pb, url.data(), flags);
        if (r < 0) throw format_context_error{"Could not open AVIO output context"};
        m_is_open_output = true;
    }
    void close_output() {
        int r = avio_closep(&m_ctx->pb);
        if (r < 0) throw format_context_error{"Could not close AVIO"};
        m_is_open_output = false;
    }


    void write_header(AVDictionary** options = nullptr) {
        int r = avformat_write_header(m_ctx, options);
        if (r < 0) throw format_context_error{"Could not write header"};
    }
    void write_trailer() {
        int r = av_write_trailer(m_ctx);
        if (r < 0) {
            throw std::runtime_error("Could not write trailer");
        }
    }
    void write_frame_interleaved(Packet& pkt) {
        int r = av_interleaved_write_frame(m_ctx, pkt.get_inner());
        if (r < 0) throw format_context_error{"COuld not write frme"};
    }

    StreamT new_stream(const AVCodec* codec = nullptr) {
        AVStream* stream = avformat_new_stream(m_ctx, codec);
        return std::shared_ptr<AVStream>{stream, StreamDeleter{}};
    }
};