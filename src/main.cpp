#include "transcode.hpp"
#include "remux.hpp"
#include "argparse/argparse.hpp"
#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    argparse::ArgumentParser parser;
    parser.add_argument("-i")
        .help("Input file");
    parser.add_argument("-o")
        .help("Output file");

    // Modes
    auto& group = parser.add_mutually_exclusive_group();
    group.add_argument("--transmux")
        .flag()
        .help("Transmuxing mode");
    group.add_argument("--transcode")
        .flag()
        .help("Transcoding mode");

    // Transmux params
    parser.add_argument("--to")
        .default_value(std::string{})
        .help("To which container format");

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
    }
    return 0;
}
