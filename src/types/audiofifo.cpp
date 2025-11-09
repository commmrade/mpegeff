#include "audiofifo.hpp"

AudioFifo::AudioFifo(enum AVSampleFormat sample_fmt, int channels, int nb_samples) {
    buf = av_audio_fifo_alloc(sample_fmt, channels, nb_samples);
    if (!buf) {
        throw std::bad_alloc{};
    }
}
AudioFifo::~AudioFifo() {
    av_audio_fifo_free(buf);
}

void AudioFifo::resize(int size) {
    int ret = av_audio_fifo_realloc(buf, size);
    if (ret < 0) {
        throw std::bad_alloc{};
    }
}

void AudioFifo::write(void * const *data, int size) {
    int ret = av_audio_fifo_write(buf, data, size);
    if (ret < 0) {
        throw audio_fifo_error{"Could not write to the buffer"};
    }
}

int AudioFifo::read(void * const *data, int size) {
    int rd = av_audio_fifo_read(buf, data, size);
    return rd;
}
