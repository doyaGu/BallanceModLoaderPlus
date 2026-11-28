#pragma once
#include "Common/ModSupport.hpp"
#include "Common/Commands.hpp"

#include "Segment.h"

class CommandSeg {
  public:
    static BML::Command::Definition Define() {
        BML::Command::Definition d;
        d.Name = "segment";
        d.Alias = "seg";
        d.Description = "Command for segment.";
        d.Cheat = false;
        return d;
    }

    explicit CommandSeg(Segment *segment);

    void Execute(const BML::Mod::Context &context, const std::vector<std::string> &args);

    const std::vector<std::string> GetTabCompletion(const BML::Mod::Context &context,
                                                    const std::vector<std::string> &args) {
        return args.size() == 2 ? std::vector<std::string>{"clear"} : std::vector<std::string>{};
    }

  private:
    Segment *segment_;
};
