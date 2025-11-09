#pragma once

#include <memory>
extern "C" {
    #include <libavutil/frame.h>
}

class Frame {
    AVFrame* m_frame{nullptr};
public:
    Frame();
    ~Frame();
    void unref();

    AVFrame *get_inner() { return m_frame; }
    const AVFrame *get_inner() const { return m_frame; }

    void make_buffer(int align = 0);
};
