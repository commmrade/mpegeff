#include "formatcontext.hpp"

FormatContextBase::FormatContextBase() {
    m_ctx = avformat_alloc_context();
    if (!m_ctx)
        throw std::bad_alloc{};
}
std::vector<AVStream *> FormatContextBase::streams() {
    std::vector<AVStream *> res;
    int n = streams_count();
    res.reserve(n);
    for (auto i = 0; i < n; ++i)
        res.push_back(m_ctx->streams[i]);
    return res;
}
InputFormatContext::InputFormatContext() {
    m_ctx = avformat_alloc_context();
    if (!m_ctx) {
        throw std::bad_alloc{};
    }
}
InputFormatContext::~InputFormatContext() { free_context(); }
void InputFormatContext::free_context() {
    if (m_is_open_input) {
        close_input();
    }
}
void InputFormatContext::open_input(const std::string_view url,
                                    const AVInputFormat *fmt,
                                    AVDictionary **options) {
    int ret = avformat_open_input(&m_ctx, url.data(), fmt, options);
    if (ret < 0)
        throw format_context_error{"Could not open input"};
    m_is_open_input = true;
}
void InputFormatContext::close_input() {
    avformat_close_input(&m_ctx);
    m_is_open_input = false;
}
int InputFormatContext::read_raw_frame(Packet &pkt) {
    return av_read_frame(m_ctx, pkt.get_inner());
}
std::optional<Packet> InputFormatContext::read_raw_frame() {
    Packet pkt;
    int r = av_read_frame(m_ctx, pkt.get_inner());
    if (r < 0)
        return {std::nullopt};
    return {std::move(pkt)};
}
void InputFormatContext::find_best_stream_info(AVDictionary **options) {
    int ret = avformat_find_stream_info(m_ctx, options);
    if (ret < 0)
        throw format_context_error{"Could not find stream info"};
}
void OutputFormatContext::free_context() {
    if (m_is_open_output) {
        close_output();
    }
}

OutputFormatContext::~OutputFormatContext() { free_context(); }

void OutputFormatContext::alloc_output(const std::string_view filename,
                                       const std::string_view format_name,
                                       const AVOutputFormat *oformat) {
    if (m_ctx) {
        free_context();
    }
    int ret = avformat_alloc_output_context2(&m_ctx, oformat, format_name.data(),
                                            filename.data());
    if (ret < 0)
        throw format_context_error{"Could not alloc output context"};
}
void OutputFormatContext::open_output(const std::string_view url, int flags) {
    int r = avio_open(&m_ctx->pb, url.data(), flags);
    if (r < 0)
        throw format_context_error{"Could not open AVIO output context"};
    m_is_open_output = true;
}
void OutputFormatContext::close_output() {
    int r = avio_closep(&m_ctx->pb);
    if (r < 0)
        throw format_context_error{"Could not close AVIO"};
    m_is_open_output = false;
}
void OutputFormatContext::write_header(AVDictionary **options) {
    int r = avformat_write_header(m_ctx, options);
    if (r < 0)
        throw format_context_error{"Could not write header"};
}
void OutputFormatContext::write_trailer() {
    int r = av_write_trailer(m_ctx);
    if (r < 0) {
        throw std::runtime_error("Could not write trailer");
    }
}
void OutputFormatContext::write_frame_interleaved(Packet &pkt) {
    int r = av_interleaved_write_frame(m_ctx, pkt.get_inner());
    if (r < 0)
        throw format_context_error{"Could not write frme"};
}
StreamT OutputFormatContext::new_stream(const AVCodec *codec) {
    AVStream *stream = avformat_new_stream(m_ctx, codec);
    return std::shared_ptr<AVStream>{stream, StreamDeleter{}};
}
