#include <BML/ICommand.h>
#include <BML/IBML.h>
#include <BML/IMod.h>

namespace {

class RejectedEntryCommand final : public ICommand {
public:
    std::string GetName() override { return "rejected-entry-test"; }
    std::string GetAlias() override { return {}; }
    std::string GetDescription() override { return "Must be removed with the rejected entry"; }
    bool IsCheat() override { return false; }
    void Execute(IBML *, const std::vector<std::string> &) override {}
    const std::vector<std::string> GetTabCompletion(IBML *, const std::vector<std::string> &) override {
        return {};
    }
};

RejectedEntryCommand g_Command;

} // namespace

BML_MOD_ENTRY(IMod *) BMLEntry(IBML *bml) {
    bml->RegisterCommand(&g_Command);
    return nullptr;
}

BML_MOD_ENTRY(void) BMLExit(IMod *) {}
