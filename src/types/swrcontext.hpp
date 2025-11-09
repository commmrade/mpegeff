extern "C" {
    #include <libswresample/swresample.h>
}

class SwrCtx {
    SwrContext* ctx{nullptr};
public:
  SwrCtx();
  SwrCtx(const AVChannelLayout *out_ch_layout,
         enum AVSampleFormat out_sample_fmt, int out_sample_rate,
         const AVChannelLayout *in_ch_layout, enum AVSampleFormat in_sample_fmt,
         int in_sample_rate);
  ~SwrCtx();
  SwrContext *get_inner() { return ctx; }
  const SwrContext *get_inner() const { return ctx; }

  int init();

  int convert(uint8_t *const *out, int out_count, const uint8_t *const *in,
              int in_count);
};
