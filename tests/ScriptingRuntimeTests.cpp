#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>

#include <angelscript.h>

#include "Core/ApiRegistration.h"
#include "Core/ApiRegistry.h"
#include "Core/BuiltinInterfaces.h"
#include "Core/ConfigStore.h"
#include "Core/Context.h"
#include "Core/CrashDumpWriter.h"
#include "Core/FaultTracker.h"
#include "Core/HookRegistry.h"
#include "Core/ImcBus.h"
#include "Core/InterfaceRegistry.h"
#include "Core/LeaseManager.h"
#include "Core/LocaleManager.h"
#include "Core/MemoryManager.h"
#include "Core/ModManifest.h"
#include "Core/ModuleLoader.h"
#include "Core/TimerManager.h"
#include "TestKernel.h"

#include "bml_export.h"

#include "../modules/BML_Scripting/src/CoroutineManager.h"
#include "../modules/BML_Scripting/src/ModuleScope.h"
#include "../modules/BML_Scripting/src/ScriptEngine.h"
#include "../modules/BML_Scripting/src/ScriptInstanceManager.h"

using BML::Core::Testing::TestKernel;

namespace {

namespace fs = std::filesystem;

fs::path CreateTempDir() {
    auto base = fs::temp_directory_path();
    auto unique = "bml-scripting-runtime-test-" +
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    auto dir = base / unique;
    fs::create_directories(dir);
    return dir;
}

class ScriptingRuntimeTest : public ::testing::Test {
protected:
    TestKernel kernel_;
    static inline ScriptingRuntimeTest *s_Current = nullptr;
    static inline BML::Scripting::CoroutineManager *s_CoroutineManager = nullptr;

    void SetUp() override {
        kernel_->api_registry = std::make_unique<BML::Core::ApiRegistry>();
        kernel_->config = std::make_unique<BML::Core::ConfigStore>();
        kernel_->memory = std::make_unique<BML::Core::MemoryManager>();
        kernel_->leases = std::make_unique<BML::Core::LeaseManager>();
        kernel_->fault_tracker = std::make_unique<BML::Core::FaultTracker>();
        kernel_->crash_dump = std::make_unique<BML::Core::CrashDumpWriter>();
        kernel_->context = std::make_unique<BML::Core::Context>(*kernel_->api_registry, *kernel_->config, *kernel_->crash_dump, *kernel_->fault_tracker);
        kernel_->config->BindContext(*kernel_->context);
        kernel_->interface_registry = std::make_unique<BML::Core::InterfaceRegistry>(*kernel_->context, *kernel_->leases);
        kernel_->imc_bus = std::make_unique<BML::Core::ImcBus>();
        kernel_->hooks = std::make_unique<BML::Core::HookRegistry>();
        kernel_->locale = std::make_unique<BML::Core::LocaleManager>();
        kernel_->timers = std::make_unique<BML::Core::TimerManager>(*kernel_->context);

        m_TempDir = CreateTempDir();

        auto &ctx = *kernel_->context;
        ctx.Initialize({0, 4, 0});

        BML::Core::RegisterCoreApis();
        BML::Core::RegisterBuiltinInterfaces();
        BML::Core::PopulateBuiltinServices(m_Builtins);
        BML::Scripting::g_Builtins = &m_Builtins;

        ASSERT_TRUE(m_Engine.Initialize());

        m_Manager = std::make_unique<BML::Scripting::ScriptInstanceManager>(m_Engine.Get());
        m_Coroutines = std::make_unique<BML::Scripting::CoroutineManager>(
            m_Engine.Get(), m_Manager.get());
        m_Manager->SetCoroutineManager(m_Coroutines.get());

        s_Current = this;
        s_CoroutineManager = m_Coroutines.get();

        ASSERT_GE(m_Engine.Get()->RegisterGlobalFunction(
            "void record(const string &in)",
            asFUNCTION(Record), asCALL_CDECL), 0);
        ASSERT_GE(m_Engine.Get()->RegisterGlobalFunction(
            "void verifyModuleContext(const string &in)",
            asFUNCTION(VerifyModuleContext), asCALL_CDECL), 0);
        ASSERT_GE(m_Engine.Get()->RegisterGlobalFunction(
            "void failReload()",
            asFUNCTION(FailReload), asCALL_CDECL), 0);
        ASSERT_GE(m_Engine.Get()->RegisterGlobalFunction(
            "void yieldForTest()",
            asFUNCTION(YieldForTest), asCALL_CDECL), 0);
    }

    void TearDown() override {
        s_CoroutineManager = nullptr;
        s_Current = nullptr;

        m_Coroutines.reset();
        m_Manager.reset();
        m_Engine.Shutdown();

        BML::Scripting::g_Builtins = nullptr;

        std::error_code ec;
        fs::remove_all(m_TempDir, ec);
    }

    BML_Mod CreateLoadedScriptModule(const std::string &modId, const fs::path &entryPath) {
        auto manifest = std::make_unique<BML::Core::ModManifest>();
        manifest->package.id = modId;
        manifest->package.name = modId;
        manifest->package.version = "1.0.0";
        manifest->package.parsed_version = {1, 0, 0};
        manifest->package.entry = entryPath.filename().string();
        manifest->directory = entryPath.parent_path().wstring();
        manifest->manifest_path = (entryPath.parent_path() / "mod.toml").wstring();

        auto *manifestRaw = manifest.get();
        auto &ctx = *kernel_->context;
        ctx.RegisterManifest(std::move(manifest));

        auto modHandle = ctx.CreateModHandle(*manifestRaw);
        if (!modHandle) {
            return nullptr;
        }
        BML_Mod mod = modHandle.get();

        BML::Core::LoadedModule loaded;
        loaded.id = modId;
        loaded.manifest = manifestRaw;
        loaded.path = entryPath.wstring();
        loaded.mod_handle = std::move(modHandle);
        ctx.AddLoadedModule(std::move(loaded));

        return mod;
    }

    void WriteScript(const fs::path &path, const std::string &content) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(out.is_open());
        out << content;
    }

    static void Record(const std::string &value) {
        if (s_Current) {
            s_Current->m_Events.push_back(value);
        }
    }

    static void VerifyModuleContext(const std::string &phase) {
        if (!s_Current) {
            return;
        }

        const bool ok = (BML::Core::Context::GetCurrentModule() == s_Current->m_Mod);
        s_Current->m_ContextPhases.push_back(ok ? phase : ("bad:" + phase));
    }

    static void FailReload() {
        if (auto *ctx = asGetActiveContext()) {
            ctx->SetException("reload veto");
        }
    }

    static void YieldForTest() {
        if (s_CoroutineManager) {
            s_CoroutineManager->YieldCurrent();
        }
    }

    fs::path m_TempDir;
    bml::BuiltinServices m_Builtins;
    BML::Scripting::ScriptEngine m_Engine;
    std::unique_ptr<BML::Scripting::ScriptInstanceManager> m_Manager;
    std::unique_ptr<BML::Scripting::CoroutineManager> m_Coroutines;
    std::vector<std::string> m_Events;
    std::vector<std::string> m_ContextPhases;
    BML_Mod m_Mod = nullptr;
};

TEST_F(ScriptingRuntimeTest, ReloadRunsOnInitWhenVirtoolsIsAlreadyReady) {
    const fs::path entryPath = m_TempDir / "main.as";
    WriteScript(entryPath, R"(
        void OnAttach() { record("attach-v1"); }
        void OnInit() { record("init-v1"); }
    )");

    m_Mod = CreateLoadedScriptModule("test.script.reload_init", entryPath);

    ASSERT_EQ(m_Manager->CompileAndAttach(
        m_Mod, bmlGetProcAddress,
        entryPath.string().c_str(),
        m_TempDir.string().c_str()), BML_RESULT_OK);

    ASSERT_EQ(m_Events.size(), 1u);
    EXPECT_EQ(m_Events[0], "attach-v1");

    m_Manager->SetVirtoolsReady(true);
    m_Manager->InitAll();

    ASSERT_EQ(m_Events.size(), 2u);
    EXPECT_EQ(m_Events[1], "init-v1");

    m_Events.clear();
    WriteScript(entryPath, R"(
        void OnAttach() { record("attach-v2"); }
        void OnInit() { record("init-v2"); }
    )");

    ASSERT_EQ(m_Manager->Reload(m_Mod), BML_RESULT_OK);

    ASSERT_EQ(m_Events.size(), 2u);
    EXPECT_EQ(m_Events[0], "attach-v2");
    EXPECT_EQ(m_Events[1], "init-v2");

    auto *inst = m_Manager->FindByMod(m_Mod);
    ASSERT_NE(inst, nullptr);
    EXPECT_EQ(inst->state, BML::Scripting::ScriptInstance::State::InitDone);
}

TEST_F(ScriptingRuntimeTest, ReloadLifecycleCallbacksRestoreCurrentModuleContext) {
    const fs::path entryPath = m_TempDir / "main.as";
    WriteScript(entryPath, R"(
        void OnAttach() { verifyModuleContext("attach-v1"); }
        void OnPrepareReload() { verifyModuleContext("prepare-v1"); }
        void OnDetach() { verifyModuleContext("detach-v1"); }
    )");

    m_Mod = CreateLoadedScriptModule("test.script.reload_context", entryPath);

    BML::Core::Context::SetCurrentModule(m_Mod);
    ASSERT_EQ(m_Manager->CompileAndAttach(
        m_Mod, bmlGetProcAddress,
        entryPath.string().c_str(),
        m_TempDir.string().c_str()), BML_RESULT_OK);
    BML::Core::Context::SetCurrentModule(nullptr);

    m_ContextPhases.clear();
    WriteScript(entryPath, R"(
        void OnAttach() { verifyModuleContext("attach-v2"); }
        void OnPrepareReload() { verifyModuleContext("prepare-v2"); }
        void OnDetach() { verifyModuleContext("detach-v2"); }
    )");

    ASSERT_EQ(m_Manager->Reload(m_Mod), BML_RESULT_OK);

    ASSERT_EQ(m_ContextPhases.size(), 3u);
    EXPECT_EQ(m_ContextPhases[0], "prepare-v1");
    EXPECT_EQ(m_ContextPhases[1], "detach-v1");
    EXPECT_EQ(m_ContextPhases[2], "attach-v2");
}

TEST_F(ScriptingRuntimeTest, FailedPrepareReloadKeepsCurrentModuleLoaded) {
    const fs::path entryPath = m_TempDir / "main.as";
    WriteScript(entryPath, R"(
        void OnPrepareReload() { failReload(); }
        void ReportVersion() { record("v1"); }
    )");

    m_Mod = CreateLoadedScriptModule("test.script.prepare_reload", entryPath);

    ASSERT_EQ(m_Manager->CompileAndAttach(
        m_Mod, bmlGetProcAddress,
        entryPath.string().c_str(),
        m_TempDir.string().c_str()), BML_RESULT_OK);

    WriteScript(entryPath, R"(
        void ReportVersion() { record("v2"); }
    )");

    EXPECT_NE(m_Manager->Reload(m_Mod), BML_RESULT_OK);

    m_Events.clear();
    ASSERT_EQ(m_Manager->InvokeByName(m_Mod, "ReportVersion"), BML_RESULT_OK);
    ASSERT_EQ(m_Events.size(), 1u);
    EXPECT_EQ(m_Events[0], "v1");
}

TEST_F(ScriptingRuntimeTest, ResumedCoroutineTimeoutMarksInstanceAsError) {
    const fs::path entryPath = m_TempDir / "main.as";
    WriteScript(entryPath, R"(
        void Start() {
            yieldForTest();
            while (true) {
            }
        }
    )");

    m_Mod = CreateLoadedScriptModule("test.script.coroutine_timeout", entryPath);

    ASSERT_EQ(m_Manager->CompileAndAttach(
        m_Mod, bmlGetProcAddress,
        entryPath.string().c_str(),
        m_TempDir.string().c_str()), BML_RESULT_OK);

    auto *inst = m_Manager->FindByMod(m_Mod);
    ASSERT_NE(inst, nullptr);
    inst->timeout_ms = 1;

    EXPECT_NE(m_Manager->InvokeByName(m_Mod, "Start"), BML_RESULT_OK);

    m_Coroutines->Tick();
    EXPECT_EQ(inst->state, BML::Scripting::ScriptInstance::State::Error);
}

} // namespace
