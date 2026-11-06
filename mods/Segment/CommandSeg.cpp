#include "CommandSeg.h"

CommandSeg::CommandSeg(Segment *segment) : segment_(segment) {}

void CommandSeg::Execute(const BML::Mod::Context &context, const std::vector<std::string> &args) {
    if (ModSupport::GameState().Ingame && args.size() > 1) {
        if (args[1] == "clear") {
            segment_->ClearHistory();
        } else if (args[1] == "show") {
            segment_->Show();
        } else if (args[1] == "hide") {
            segment_->Hide();
        }
    }
}
