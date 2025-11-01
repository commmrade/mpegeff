#include "exceptions.hpp"
#include <new>
extern "C" {
    #include <libavutil/audio_fifo.h>
}

class AudioFifo {
    AVAudioFifo* buf{nullptr};
public:
    AudioFifo(enum AVSampleFormat sample_fmt, int channels, int nb_samples) {
        buf = av_audio_fifo_alloc(sample_fmt, channels, nb_samples);
        if (!buf) {
            throw std::bad_alloc{};
        }
    }
    ~AudioFifo() {
        av_audio_fifo_free(buf);
    }
    AVAudioFifo* get_inner() {
        return buf;
    }

    int remaining_size() const {
        return av_audio_fifo_size(buf);
    }

    void resize(int size) {
        int ret = av_audio_fifo_realloc(buf, size);
        if (ret < 0) {
            throw std::bad_alloc{};
        }
    }

    void write(void * const *data, int size) {
        int ret = av_audio_fifo_write(buf, data, size);
        if (ret < 0) {
            throw audio_fifo_error{"Could not write to the buffer"};
        }
    }

    int read(void * const *data, int size) {
        int rd = av_audio_fifo_read(buf, data, size);
        return rd;
    }
};
