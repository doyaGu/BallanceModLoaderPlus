#include "CommandSeg.h"

CommandSeg::CommandSeg(Segment *segment): segment_(segment) {}

void CommandSeg::Execute(IBML *bml, const std::vector<std::string> &args) {
    if (bml->IsIngame() && args.size() > 1) {
        if (args[1] == "clear") {
            segment_->ClearHistory();
        } else if (args[1] == "show") {
            segment_->Show();
        } else if (args[1] == "hide") {
            segment_->Hide();
        }
    }
}
