#pragma once
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>
#include "exceptions.hpp"
#include "stream.hpp"
#include "packet.hpp"
extern "C" {
    #include <libavformat/avio.h>
    #include <libavutil/dict.h>
    #include <libavformat/avformat.h>
}


class FormatContextBase {
protected:
    AVFormatContext* m_ctx{nullptr};
public:
    FormatContextBase();
    virtual ~FormatContextBase() { avformat_free_context(m_ctx); }

    unsigned int streams_count() const { return m_ctx->nb_streams; }
    std::vector<AVStream *> streams();

    AVFormatContext *get_inner() { return m_ctx; }
    const AVFormatContext *get_inner() const { return m_ctx; }
};

class InputFormatContext : public FormatContextBase {
    bool m_is_open_input{false};
public:
    InputFormatContext();
    ~InputFormatContext() override;
    void free_context();

    // input output
    void open_input(const std::string_view url,
                    const AVInputFormat *fmt = nullptr,
                    AVDictionary **options = nullptr);
    void close_input();

    int read_raw_frame(Packet &pkt);
    std::optional<Packet> read_raw_frame();

    void find_best_stream_info(AVDictionary **options = nullptr);
};



class OutputFormatContext : public FormatContextBase {
    bool m_is_open_output{false};
public:
    OutputFormatContext() = default;
    ~OutputFormatContext() override;
    void free_context();

    void alloc_output(const std::string_view filename,
                      const std::string_view format_name = {},
                      const AVOutputFormat *oformat = nullptr);
    void open_output(const std::string_view url, int flags);
    void close_output();

    void write_header(AVDictionary **options = nullptr);
    void write_trailer();
    void write_frame_interleaved(Packet &pkt);

    StreamT new_stream(const AVCodec *codec = nullptr);
};
