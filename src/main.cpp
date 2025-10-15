#include "transcode.hpp"
#include "remux.hpp"


int main() {
    remux("edit.mp4", "tmuxed_edit.mov");
    IContext i{};
    i.filepath = "edit.mp4";
    OContext o{};
    o.filepath = "svo.mp4";
    transcode("edit.mp4", "svo.mp4", i, o);
    return 0;
}