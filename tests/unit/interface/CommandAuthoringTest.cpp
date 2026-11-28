#include "BML/Command.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

struct HostState {
    bool Registered = false;
    bool Enabled = true;
    bool ReturnInvalidHandle = false;
    int UnregisterStatus = BML_OK;
    int ContextStatus = BML_OK;
    BML_CommandHandle NextHandle = 0;
    BML_CommandHandle Handle = BML_COMMAND_INVALID_HANDLE;
    std::string Name;
    std::string Alias;
    std::string Description;
    std::string Usage;
    std::string Category;
    BML_CommandFlags Flags = BML_COMMAND_NONE;
    void *UserData = nullptr;
    BML_CommandExecute Execute = nullptr;
    BML_CommandComplete Complete = nullptr;
    BML_CommandRelease Release = nullptr;
};

HostState g_Host;

void ResetHost() {
    g_Host = HostState{};
}

void ReleaseHostRegistration() {
    BML_CommandRelease release = g_Host.Release;
    void *userData = g_Host.UserData;
    g_Host.Registered = false;
    g_Host.UserData = nullptr;
    g_Host.Execute = nullptr;
    g_Host.Complete = nullptr;
    g_Host.Release = nullptr;
    if (release)
        release(userData);
}

int BML_CDECL RegisterCommand(const char *, const BML_CommandDefinition *definition,
                              BML_CommandHandle *outHandle) {
    if (!definition || !outHandle || !definition->Name || !definition->Execute || g_Host.Registered)
        return BML_ERROR_INVALID_PARAMETER;

    g_Host.Registered = true;
    g_Host.Enabled = (definition->Flags & BML_COMMAND_DISABLED) == 0;
    g_Host.Handle = g_Host.ReturnInvalidHandle ? BML_COMMAND_INVALID_HANDLE : ++g_Host.NextHandle;
    g_Host.Name = definition->Name;
    g_Host.Alias = definition->Alias ? definition->Alias : "";
    g_Host.Description = definition->Description ? definition->Description : "";
    g_Host.Usage = definition->Usage ? definition->Usage : "";
    g_Host.Category = definition->Category ? definition->Category : "";
    g_Host.Flags = definition->Flags;
    g_Host.UserData = definition->UserData;
    g_Host.Execute = definition->Execute;
    g_Host.Complete = definition->Complete;
    g_Host.Release = definition->Release;
    *outHandle = g_Host.Handle;
    return BML_OK;
}

int BML_CDECL UnregisterCommand(const char *, BML_CommandHandle handle) {
    if (!g_Host.Registered || handle != g_Host.Handle)
        return BML_ERROR_INVALID_HANDLE;
    if (g_Host.UnregisterStatus != BML_OK)
        return g_Host.UnregisterStatus;
    ReleaseHostRegistration();
    return BML_OK;
}

int BML_CDECL SetCommandEnabled(const char *, BML_CommandHandle handle, int enabled) {
    if (!g_Host.Registered || handle != g_Host.Handle)
        return BML_ERROR_INVALID_HANDLE;
    g_Host.Enabled = enabled != 0;
    return BML_OK;
}

BML_CommandInfo MakeInfo() {
    return BML_CommandInfo{
        sizeof(BML_CommandInfo),    g_Host.Handle,
        g_Host.Name.c_str(),        g_Host.Alias.c_str(),
        g_Host.Description.c_str(), g_Host.Usage.c_str(),
        g_Host.Category.c_str(),    g_Host.Flags,
    };
}

int BML_CDECL VisitCommands(BML_CommandVisitor visitor, void *userData) {
    if (!visitor)
        return BML_ERROR_INVALID_PARAMETER;
    if (!g_Host.Registered)
        return BML_OK;
    const BML_CommandInfo info = MakeInfo();
    return visitor(userData, &info);
}

int BML_CDECL FindCommand(const char *name, BML_CommandVisitor visitor, void *userData) {
    if (!name || !visitor)
        return BML_ERROR_INVALID_PARAMETER;
    if (!g_Host.Registered || (g_Host.Name != name && g_Host.Alias != name))
        return BML_ERROR_NOT_FOUND;
    const BML_CommandInfo info = MakeInfo();
    return visitor(userData, &info);
}

int BML_CDECL ExecuteLine(const char *, int *outStatus) {
    if (!outStatus || !g_Host.Registered || !g_Host.Execute)
        return BML_ERROR_NOT_FOUND;

    BML_CommandExecute execute = g_Host.Execute;
    void *userData = g_Host.UserData;
    const BML_CommandInvocation invocation = {
        sizeof(BML_CommandInvocation),
        g_Host.Name.c_str(),
        g_Host.Alias.empty() ? g_Host.Name.c_str() : g_Host.Alias.c_str(),
        0,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
    };
    *outStatus = execute(userData, &invocation);
    return BML_OK;
}

const BML_CommandInterface CommandInterface = {
    BML_IFACE_HEADER(BML_CommandInterface, BML_COMMAND_INTERFACE_ID, BML_COMMAND_INTERFACE_MAJOR,
                     BML_COMMAND_INTERFACE_MINOR),
    &RegisterCommand,
    &UnregisterCommand,
    &SetCommandEnabled,
    &VisitCommands,
    &FindCommand,
    &ExecuteLine,
};

int BML_CDECL AddCandidate(void *context, const char *text, std::size_t length) {
    if (!context || !text)
        return BML_ERROR_INVALID_PARAMETER;
    static_cast<std::vector<std::string> *>(context)->emplace_back(text, length);
    return BML_OK;
}

BML::Mod::Context Context() {
    return BML::Mod::Detail::ContextAccess::Make(42);
}

BML::Result<void> Register(BML::Command::Registration &registration,
                           const BML::Command::Definition &definition,
                           BML::Command::ExecuteCallback execute,
                           BML::Command::CompleteCallback complete = {}) {
    auto result =
        Context().Commands().Register(definition, std::move(execute), std::move(complete));
    if (!result)
        return BML::Result<void>::Failure(result.Code());
    registration = result.Take();
    return BML::Result<void>::Success();
}

int ExecuteCommand(std::string_view line, int &out) {
    auto result = Context().Commands().ExecuteLine(line);
    out = result ? result.Value() : BML_COMMAND_STATUS_FAILURE;
    return result.Code();
}

BML_ModContextInterface ContextInterface() {
    BML_ModContextInterface api{};
    api.Header = BML_IFACE_HEADER(BML_ModContextInterface, BML_MOD_CONTEXT_INTERFACE_ID, 1, 0);
    api.GetOwner = +[](BML_ModContextHandle, BML_ModHandle *out) -> int {
        if (g_Host.ContextStatus != BML_OK)
            return g_Host.ContextStatus;
        *out = 1;
        return BML_OK;
    };
    api.ReadMod =
        +[](BML_ModContextHandle, BML_ModHandle, BML_ModVisitor visitor, void *data) -> int {
        const BML_ModDefinition definition{
            sizeof(definition), "test.owner", "Test", "1.0.0", "", "", "0.4.0", nullptr, 0, 0};
        const BML_ModInfo info{sizeof(info),         1,           BML_MOD_KIND_NATIVE,
                               BML_MOD_STATE_LOADED, &definition, ""};
        return visitor(data, &info);
    };
    api.FindMod = +[](BML_ModContextHandle, const char *, BML_ModHandle *out) -> int {
        *out = 1;
        return BML_OK;
    };
    api.VisitMods =
        +[](BML_ModContextHandle, BML_ModVisitor, void *) -> int { return BML_ERROR_UNAVAILABLE; };
    api.Log = +[](BML_ModContextHandle, uint32_t, const char *, size_t) -> int {
        return BML_ERROR_UNAVAILABLE;
    };
    api.GetDirectory = +[](BML_ModContextHandle, uint32_t, char *, size_t, size_t *) -> int {
        return BML_ERROR_UNAVAILABLE;
    };
    api.GetResourcePath = +[](BML_ModContextHandle, const char *, char *, size_t, size_t *) -> int {
        return BML_ERROR_UNAVAILABLE;
    };
    api.ReadResource = +[](BML_ModContextHandle, const char *, void *, size_t, size_t *) -> int {
        return BML_ERROR_UNAVAILABLE;
    };
    api.ResourceExists = +[](BML_ModContextHandle, const char *, uint32_t *) -> int {
        return BML_ERROR_UNAVAILABLE;
    };
    return api;
}

const auto ContextApi = ContextInterface();

} // namespace

extern "C" int BML_CDECL BML_GetInterface(const char *interfaceId, std::uint16_t majorVersion,
                                          const void **out) {
    if (!interfaceId || !out)
        return BML_ERROR_INVALID_PARAMETER;
    *out = nullptr;
    if (std::strcmp(interfaceId, BML_MOD_CONTEXT_INTERFACE_ID) == 0) {
        *out = &ContextApi;
        return BML_OK;
    }
    if (std::strcmp(interfaceId, BML_COMMAND_INTERFACE_ID) != 0)
        return BML_ERROR_NOT_FOUND;
    if (majorVersion != BML_COMMAND_INTERFACE_MAJOR)
        return BML_ERROR_VERSION_MISMATCH;
    *out = &CommandInterface;
    return BML_OK;
}

TEST(CommandAuthoringTest, CopiesMetadataAndFindsRegisteredCommand) {
    ResetHost();
    BML::Command::Definition definition;
    definition.Name = "inspect";
    definition.Alias = "i";
    definition.Description = "Inspect a command";
    definition.Usage = "inspect <name>";
    definition.Category = "test";
    definition.Hidden = true;

    BML::Command::Registration registration;
    ASSERT_EQ(Register(registration, definition, [](const BML::Command::Invocation &) { return 0; })
                  .Code(),
              BML_OK);

    BML::Command::Info info;
    auto found = Context().Commands().Find("i");
    ASSERT_TRUE(found);
    info = found.Take();
    EXPECT_EQ(info.Handle, registration.Handle());
    EXPECT_EQ(info.Name, definition.Name);
    EXPECT_EQ(info.Alias, definition.Alias);
    EXPECT_EQ(info.Description, definition.Description);
    EXPECT_EQ(info.Usage, definition.Usage);
    EXPECT_EQ(info.Category, definition.Category);
    EXPECT_TRUE(info.Hidden);
    EXPECT_TRUE(info.Enabled);
}

TEST(CommandAuthoringTest, CommandSnapshotsOwnMetadataAfterUnregister) {
    ResetHost();
    BML::Command::Definition definition;
    definition.Name = "snapshot";
    definition.Description = std::string(128, 'x');
    auto registration =
        Context()
            .Commands()
            .Register(definition, [](const BML::Command::Invocation &) { return 0; })
            .Take();

    auto commands = Context().Commands().ReadCommands();
    ASSERT_TRUE(commands);
    ASSERT_EQ(commands.Value().size(), 1u);
    registration.Unregister().Take();
    ResetHost();
    EXPECT_EQ(commands.Value()[0].Name, "snapshot");
    EXPECT_EQ(commands.Value()[0].Description, definition.Description);
    EXPECT_TRUE(Context().Commands().ReadCommands().Value().empty());
}

TEST(CommandAuthoringTest, QueriesPropagateContextFailureBeforeCallingTheCommandHost) {
    ResetHost();
    for (const int code : {BML_ERROR_WRONG_THREAD, BML_ERROR_FROZEN, BML_ERROR_ACCESS_DENIED}) {
        g_Host.ContextStatus = code;
        EXPECT_EQ(Context().Commands().Find("missing").Code(), code);
        EXPECT_EQ(Context().Commands().ReadCommands().Code(), code);
        EXPECT_EQ(Context().Commands().ExecuteLine("missing").Code(), code);
    }
}

TEST(CommandAuthoringTest, CallbackStateLivesUntilTheHostReleasesRegistration) {
    ResetHost();
    auto marker = std::make_shared<int>(1);
    std::weak_ptr<int> lifetime = marker;
    BML::Command::Definition definition;
    definition.Name = "lifetime";
    BML::Command::Registration registration;
    ASSERT_EQ(Register(registration, definition,
                       [marker](const BML::Command::Invocation &) { return *marker; })
                  .Code(),
              BML_OK);

    marker.reset();
    EXPECT_FALSE(lifetime.expired());
    EXPECT_EQ(registration.Unregister().Code(), BML_OK);
    EXPECT_TRUE(lifetime.expired());
}

TEST(CommandAuthoringTest, DistinguishesAnEmptyPipeFromNoPipe) {
    ResetHost();
    BML::Command::Definition definition;
    definition.Name = "input";
    std::vector<bool> hasInput;
    BML::Command::Registration registration;
    ASSERT_EQ(Register(registration, definition,
                       [&hasInput](const BML::Command::Invocation &invocation) {
                           hasInput.push_back(invocation.HasInput());
                           EXPECT_TRUE(invocation.Input().empty());
                           return BML_COMMAND_STATUS_SUCCESS;
                       })
                  .Code(),
              BML_OK);

    const BML_CommandInvocation noPipe = {
        sizeof(BML_CommandInvocation), "input", "input", 0, nullptr, nullptr, 0, nullptr, nullptr,
    };
    const char emptyInput[] = "";
    const BML_CommandInvocation emptyPipe = {
        sizeof(BML_CommandInvocation),
        "input",
        "input",
        0,
        nullptr,
        emptyInput,
        0,
        nullptr,
        nullptr,
    };
    ASSERT_NE(g_Host.Execute, nullptr);
    EXPECT_EQ(g_Host.Execute(g_Host.UserData, &noPipe), BML_COMMAND_STATUS_SUCCESS);
    EXPECT_EQ(g_Host.Execute(g_Host.UserData, &emptyPipe), BML_COMMAND_STATUS_SUCCESS);
    ASSERT_EQ(hasInput.size(), 2u);
    EXPECT_FALSE(hasInput[0]);
    EXPECT_TRUE(hasInput[1]);
}

TEST(CommandAuthoringTest, RejectsStringsThatTheCInterfaceWouldTruncate) {
    ResetHost();
    BML::Command::Definition definition;
    definition.Name = std::string("hidden\0suffix", 13);
    BML::Command::Registration invalidRegistration;
    EXPECT_EQ(Register(invalidRegistration, definition,
                       [](const BML::Command::Invocation &) { return 0; })
                  .Code(),
              BML_ERROR_INVALID_PARAMETER);
    EXPECT_FALSE(g_Host.Registered);

    definition.Name = "visible";
    BML::Command::Registration registration;
    ASSERT_EQ(Register(registration, definition, [](const BML::Command::Invocation &) { return 0; })
                  .Code(),
              BML_OK);

    BML::Command::Info info;
    const std::string truncatedName("visible\0suffix", 14);
    EXPECT_EQ(Context().Commands().Find(truncatedName).Code(), BML_ERROR_INVALID_PARAMETER);
    int commandStatus = 99;
    EXPECT_EQ(ExecuteCommand(truncatedName, commandStatus), BML_ERROR_INVALID_PARAMETER);
    EXPECT_EQ(commandStatus, BML_COMMAND_STATUS_FAILURE);
}

TEST(CommandAuthoringTest, CanDestroyRegistrationInsideItsCallback) {
    ResetHost();
    auto registration = std::make_unique<BML::Command::Registration>();
    BML::Command::Definition definition;
    definition.Name = "self-close";

    ASSERT_EQ(Register(*registration, definition,
                       [&registration](const BML::Command::Invocation &) {
                           registration.reset();
                           return 23;
                       })
                  .Code(),
              BML_OK);

    int commandStatus = 0;
    EXPECT_EQ(ExecuteCommand("self-close", commandStatus), BML_OK);
    EXPECT_EQ(commandStatus, 23);
    EXPECT_FALSE(registration);
    EXPECT_FALSE(g_Host.Registered);
}

TEST(CommandAuthoringTest, MoveKeepsTheRegisteredCallbackStable) {
    ResetHost();
    BML::Command::Definition definition;
    definition.Name = "move";
    BML::Command::Registration original;
    ASSERT_EQ(
        Register(original, definition, [](const BML::Command::Invocation &) { return 31; }).Code(),
        BML_OK);
    const BML_CommandHandle handle = original.Handle();

    BML::Command::Registration moved(std::move(original));
    EXPECT_FALSE(original.IsRegistered());
    EXPECT_TRUE(moved.IsRegistered());
    EXPECT_EQ(moved.Handle(), handle);

    int commandStatus = 0;
    EXPECT_EQ(ExecuteCommand("move", commandStatus), BML_OK);
    EXPECT_EQ(commandStatus, 31);
}

TEST(CommandAuthoringTest, MoveAssignmentDoesNotTransferTheTargetToTheSource) {
    ResetHost();
    BML::Command::Definition targetDefinition;
    targetDefinition.Name = "target";
    BML::Command::Registration target;
    ASSERT_EQ(
        Register(target, targetDefinition, [](const BML::Command::Invocation &) { return 40; })
            .Code(),
        BML_OK);

    // Model a registration already removed by owner cleanup before its C++
    // wrapper is assigned another registration.
    ReleaseHostRegistration();
    BML::Command::Definition definition;
    definition.Name = "source";
    BML::Command::Registration source;
    ASSERT_EQ(
        Register(source, definition, [](const BML::Command::Invocation &) { return 41; }).Code(),
        BML_OK);

    target = std::move(source);

    EXPECT_FALSE(source.IsRegistered());
    EXPECT_TRUE(target.IsRegistered());
    int commandStatus = 0;
    EXPECT_EQ(ExecuteCommand("source", commandStatus), BML_OK);
    EXPECT_EQ(commandStatus, 41);
}

TEST(CommandAuthoringTest, FailedDestructorCleanupDisablesRegistration) {
    ResetHost();
    g_Host.UnregisterStatus = BML_ERROR_BUSY;
    {
        BML::Command::Definition definition;
        definition.Name = "busy";
        BML::Command::Registration registration;
        ASSERT_EQ(
            Register(registration, definition, [](const BML::Command::Invocation &) { return 17; })
                .Code(),
            BML_OK);
    }

    ASSERT_TRUE(g_Host.Registered);
    EXPECT_FALSE(g_Host.Enabled);
    ReleaseHostRegistration();
}

TEST(CommandAuthoringTest, HostCleanupBeforeWrapperDestructionReleasesCapturedStateOnce) {
    ResetHost();
    auto alive = std::make_shared<int>(0);
    {
        BML::Command::Definition definition;
        definition.Name = "host-cleanup";
        BML::Command::Registration registration;
        ASSERT_EQ(Register(registration, definition,
                           [hold = alive](const BML::Command::Invocation &) { return *hold; })
                      .Code(),
                  BML_OK);
        EXPECT_EQ(alive.use_count(), 2);
        ReleaseHostRegistration();
        EXPECT_FALSE(registration.IsRegistered());
        EXPECT_EQ(registration.Unregister().Code(), BML_OK);
    }
    EXPECT_EQ(alive.use_count(), 1);
}

TEST(CommandAuthoringTest, RejectsAProviderThatReturnsAnInvalidHandle) {
    ResetHost();
    g_Host.ReturnInvalidHandle = true;
    BML::Command::Definition definition;
    definition.Name = "invalid-handle";
    BML::Command::Registration registration;

    EXPECT_EQ(
        Register(registration, definition, [](const BML::Command::Invocation &) { return 17; })
            .Code(),
        BML_ERROR_MALFORMED_MESSAGE);
    EXPECT_FALSE(registration.IsRegistered());

    const BML_CommandInvocation invocation = {
        sizeof(BML_CommandInvocation),
        "invalid-handle",
        "invalid-handle",
        0,
        nullptr,
        nullptr,
        0,
        nullptr,
        nullptr,
    };
    ASSERT_NE(g_Host.Execute, nullptr);
    EXPECT_EQ(g_Host.Execute(g_Host.UserData, &invocation), BML_COMMAND_STATUS_FAILURE);
    ReleaseHostRegistration();
}

TEST(CommandAuthoringTest, CompletionHasDedicatedCursorContext) {
    ResetHost();
    BML::Command::Definition definition;
    definition.Name = "open";
    BML::Command::Registration registration;
    bool called = false;
    ASSERT_EQ(Register(
                  registration, definition, [](const BML::Command::Invocation &) { return 0; },
                  [&called](const BML::Command::CompletionRequest &request,
                            const BML::Command::Completion &completion) {
                      called = true;
                      EXPECT_EQ(request.Name(), "open");
                      EXPECT_EQ(request.InvokedAs(), "o");
                      if (request.Arguments().size() != 2u)
                          return BML::Result<void>::Failure(BML_ERROR_FAIL);
                      EXPECT_EQ(request.Argument(0), "folder");
                      EXPECT_EQ(request.Argument(1), "pa");
                      EXPECT_EQ(request.ActiveArgument(), 1u);
                      EXPECT_EQ(request.Prefix(), "pa");
                      return completion.Add("path");
                  })
                  .Code(),
              BML_OK);

    const char *arguments[] = {"folder", "pa"};
    const BML_CommandCompletionRequest request = {
        sizeof(BML_CommandCompletionRequest), "open", "o", 2, arguments, 1, arguments[1],
    };
    std::vector<std::string> candidates;
    const BML_CommandCompletion completion = {
        sizeof(BML_CommandCompletion),
        &candidates,
        &AddCandidate,
    };
    ASSERT_NE(g_Host.Complete, nullptr);
    EXPECT_EQ(g_Host.Complete(g_Host.UserData, &request, &completion), BML_OK);
    EXPECT_TRUE(called);
    ASSERT_EQ(candidates.size(), 1u);
    EXPECT_EQ(candidates.front(), "path");
}
