#include "algos/transcode.hpp"
#include "algos/remux.hpp"
#include "argparse/argparse.hpp"
#include <exception>

void setup_arguments(argparse::ArgumentParser& parser) {
    parser.add_argument("-i")
        .help("Input file");
    parser.add_argument("-o")
        .help("Output file");

    // Modes
    {
        auto& group = parser.add_mutually_exclusive_group();
        group.add_argument("--transmux")
            .flag()
            .help("Transmuxing mode");
        group.add_argument("--transcode")
            .flag()
            .help("Transcoding mode");
    }

    // Transmux params
    {
        parser.add_argument("--to")
            .default_value(std::string{})
            .help("To which container format");
    }

    // Transcode params
    {
        auto& audio_group = parser.add_mutually_exclusive_group();
        audio_group.add_argument("--tm-audio")
            .flag()
            .help("Don't transcode audio, just transmux (in transcoding mode)");
        audio_group.add_argument("--codec-audio")
            .default_value(std::string{"aac"})
            .help("Audio codec");

        auto& video_group = parser.add_mutually_exclusive_group();
        video_group.add_argument("--tm-video")
            .flag()
            .help("Don't transcode video, just transmux (in transmuxing mode)");
        video_group.add_argument("--codec-video")
            .default_value(std::string{"libsvtav1"})
            .help("Video codec");
    }
}


int main(int argc, char** argv) {
    argparse::ArgumentParser parser;
    setup_arguments(parser);

    try {
        parser.parse_args(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
    }

    bool is_transmuxing = parser.is_used("transmux");
    bool is_transcoding = parser.is_used("transcode");
    auto i_path = parser.get<std::string>("i");
    auto o_path = parser.get<std::string>("o");

    if (is_transmuxing) {
        auto to_ctr = parser.get<std::string>("to");
        try {
            remux(i_path, o_path, to_ctr);
        } catch (const std::exception& ex) {
            std::cerr << "Caught exception when transmuxing: " << ex.what() << std::endl;
        }
    } else if (is_transcoding) {
        IContext i;
        i.filepath = std::move(i_path);
        i.mux_audio = parser.is_used("tm-audio");
        i.mux_video = parser.is_used("tm-video");

        OContext o;
        o.filepath = std::move(o_path);

        auto codec_audio = parser.get<std::string>("codec-audio");
        auto codec_video = parser.get<std::string>("codec-video");

        try {
            transcode(i, o, codec_audio, codec_video);
        } catch (const std::exception& ex) {
            std::cerr << "Caught exception when transcoding: " << ex.what() << std::endl;
        }
    }
    return 0;
}
