#pragma once
#include <memory>
extern "C" {
    #include <libavformat/avformat.h>
}

struct StreamDeleter {
    void operator()(AVStream* stream) {
        // do nothing, cleaned up in fmt ctx destructor
    }
};

using StreamT = std::shared_ptr<AVStream>;