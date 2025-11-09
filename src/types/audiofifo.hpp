#include "exceptions.hpp"
#include <new>
extern "C" {
    #include <libavutil/audio_fifo.h>
}

class AudioFifo {
    AVAudioFifo* buf{nullptr};
public:
    AudioFifo(enum AVSampleFormat sample_fmt, int channels, int nb_samples);
    ~AudioFifo();

    AVAudioFifo* get_inner() {
        return buf;
    }

    int remaining_size() const {
        return av_audio_fifo_size(buf);
    }
    void resize(int size);

    void write(void * const *data, int size);
    int read(void * const *data, int size);
};
