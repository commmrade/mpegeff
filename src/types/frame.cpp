#include "frame.hpp"

Frame::Frame() {
    m_frame = av_frame_alloc();
    if (!m_frame) {
        throw std::bad_alloc{};
    }
}
Frame::~Frame() { av_frame_free(&m_frame); }

void Frame::unref() { av_frame_unref(m_frame); }

void Frame::make_buffer(int align) {
    int ret = av_frame_get_buffer(m_frame, align);
    if (ret < 0) {
        throw std::bad_alloc{};
    }
}
