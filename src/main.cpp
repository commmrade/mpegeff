#include "transcode.hpp"
#include "remux.hpp"
#include "argparse/argparse.hpp"

int main(int argc, char** argv) {
    // argparse::ArgumentParser parser{};

    // std::string fuck_p;
    // int a;
    // parser.add_argument("--fuck")
    //     .default_value(std::string{"noone"})
    //     .help("Who to fuck");
    // parser.add_argument("--age")
    //     .scan<'i', int>()
    //     .default_value(0)
    //     .help("Age");

    // try {
    //     parser.parse_args(argc, argv);
    // } catch (const std::exception& ex) {
    //     std::cerr << ex.what() << " " << parser << std::endl;
    // }


    // if (parser.is_used("--fuck")) {
    //     throw 1;
    // }

    // try {
    //     remux("edit.mp4", "tmuxed_edit.mov");
    // } catch (const std::exception& ex) {
    //     std::cerr << "Transmuxing failed: " << ex.what() << std::endl;
    // }

    IContext i{};
    i.filepath = "edit.mp4";
    OContext o{};
    o.filepath = "svo.mp4";
    try {
        transcode("edit.mp4", "svo.mp4", i, o);
    } catch (const std::exception& ex) {
        std::cerr << "Transcoding failed: " << ex.what() << std::endl;
    }
    return 0;
}
