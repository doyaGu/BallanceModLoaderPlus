#pragma once
#include <BML/BMLAll.h>

#include "Segment.h"

class CommandSeg : public ICommand {
public:
    explicit CommandSeg(Segment *segment);

    std::string GetName() override { return "segment"; }
    std::string GetAlias() override { return "seg"; }
    std::string GetDescription() override { return "Command for segment."; }
    bool IsCheat() override { return false; }

    void Execute(IBML *bml, const std::vector<std::string> &args) override;

    const std::vector<std::string> GetTabCompletion(IBML *bml, const std::vector<std::string> &args) override {
        return args.size() == 2 ? std::vector<std::string>{"clear"} : std::vector<std::string>{};
    }

private:
    Segment *segment_;
};
