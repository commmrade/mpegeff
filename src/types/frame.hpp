#pragma once

#include <memory>
extern "C" {
    #include <libavutil/frame.h>
}

class Frame {
    AVFrame* m_frame{nullptr};
public:
    Frame() {
        m_frame = av_frame_alloc();
        if (!m_frame) {
            throw std::bad_alloc{};
        }
    }
    ~Frame() {
        av_frame_free(&m_frame);
    }
    void unref() {
        av_frame_unref(m_frame);
    }

    AVFrame* get_inner() {
        return m_frame;
    }
    const AVFrame* get_inner() const {
        return m_frame;
    }

    void make_buffer(int align = 0) {
        int ret = av_frame_get_buffer(m_frame, align);
        if (ret < 0) {
            throw std::bad_alloc{};
        }
    }
};

// struct FrameDealloc {
//     void operator()(AVFrame* f) {
//         av_frame_free(&f);
//     }
// };
// using FrameT = std::unique_ptr<AVFrame, FrameDealloc>;
