#include "swrcontext.hpp"

SwrCtx::SwrCtx() {
    ctx = swr_alloc();
    if (!ctx) {
        throw std::bad_alloc{};
    }
}
SwrCtx::SwrCtx(const AVChannelLayout *out_ch_layout,
               enum AVSampleFormat out_sample_fmt, int out_sample_rate,
               const AVChannelLayout *in_ch_layout,
               enum AVSampleFormat in_sample_fmt, int in_sample_rate) {
    swr_alloc_set_opts2(&ctx, out_ch_layout, out_sample_fmt, out_sample_rate,
                        in_ch_layout, in_sample_fmt, in_sample_rate, 0, nullptr);
}
SwrCtx::~SwrCtx() { swr_free(&ctx); }
int SwrCtx::init() { return swr_init(ctx); }
int SwrCtx::convert(uint8_t *const *out, int out_count,
                    const uint8_t *const *in, int in_count) {
    return swr_convert(ctx, out, out_count, in, in_count);
}
