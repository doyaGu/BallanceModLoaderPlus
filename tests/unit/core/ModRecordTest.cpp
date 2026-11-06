#include <gtest/gtest.h>

#include <vector>

#include "Loader/ModRecord.h"
#include "Config/Config.h"

namespace {
struct LocalMod {
    ModRecord Registration;
    std::vector<int> Calls;
    bool Cheat = false;
    Property *Changed = nullptr;

    LocalMod() { Registration.Bind(*this); }

    const char *GetID() const { return "test.local"; }

    const char *GetName() const { return "Local Mod"; }

    const char *GetVersion() const { return "1.0.0"; }

    const char *GetAuthor() const { return "tests"; }

    const char *GetDescription() const { return ""; }

    BMLVersion GetBMLVersion() const { return {}; }

    void OnLoad() { Calls.push_back(1); }

    void OnProcess() {
        Calls.push_back(2);
        Registration.Notify<ModEvent::OnCheatEnabled>(true);
        Calls.push_back(4);
    }

    void OnCheatEnabled(bool enabled) {
        Cheat = enabled;
        Calls.push_back(3);
    }

    void OnUnload() { Calls.push_back(5); }

    void OnModifyConfig(const char *, const char *, Property *property) { Changed = property; }

    void OnRender() {} // A different signature does not subscribe to the Render event.
};

TEST(ModRecord, RegistersOnlyDeclaredMatchingCallbacks) {
    LocalMod mod;
    EXPECT_TRUE(mod.Registration.Subscribed<ModEvent::OnLoad>());
    EXPECT_TRUE(mod.Registration.Subscribed<ModEvent::OnProcess>());
    EXPECT_FALSE(mod.Registration.Subscribed<ModEvent::OnRender>());
    EXPECT_FALSE(mod.Registration.Subscribed<ModEvent::OnPhysicalize>());
    EXPECT_FALSE(mod.Registration.Subscribed<ModEvent::OnStartLevel>());
    mod.Registration.Notify<ModEvent::OnRender>(static_cast<CK_RENDER_FLAGS>(0));
}

TEST(ModRecord, NestedDispatchKeepsOrderAndArguments) {
    LocalMod mod;
    mod.Registration.Notify<ModEvent::OnLoad>();
    mod.Registration.Notify<ModEvent::OnProcess>();
    mod.Registration.Notify<ModEvent::OnUnload>();
    EXPECT_EQ((std::vector<int>{1, 2, 3, 4, 5}), mod.Calls);
    EXPECT_TRUE(mod.Cheat);
}

TEST(ModRecord, ConfigPayloadRemainsBorrowedForTheCallback) {
    LocalMod mod;
    Property property;
    mod.Registration.Notify<ModEvent::OnModifyConfig>("General", "Count", &property);
    EXPECT_EQ(&property, mod.Changed);
    EXPECT_STREQ("test.local", mod.Registration.GetID());
    EXPECT_STREQ("Local Mod", mod.Registration.GetName());
}
} // namespace
