/**
 * @file BMLIntegrationTests.cpp
 * @brief Integration tests for the clean-break runtime contract.
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#define BML_LOADER_IMPLEMENTATION
#include "bml_loader.h"

#include "bml_bootstrap.h"
#include "bml_services.hpp"
#include "bml_export.h"
#include "bml_imc.h"
#include "bml_interface.h"

#include "Core/ModHandle.h"

namespace {
    std::string ReadFileUtf8(const std::filesystem::path &path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream.is_open()) {
            return {};
        }
        std::ostringstream buffer;
        buffer << stream.rdbuf();
        return buffer.str();
    }

    class BMLIntegrationTest : public ::testing::Test {
    protected:
        void SetUp() override {
            BML_RuntimeConfig config = BML_RUNTIME_CONFIG_INIT;
            ASSERT_EQ(BML_RESULT_OK, bmlRuntimeCreate(&config, &m_Runtime));
            ASSERT_NE(nullptr, m_Runtime);

            m_Services = bmlRuntimeGetServices(m_Runtime);
            ASSERT_NE(nullptr, m_Services);
            bmlBindServices(m_Services);
        }

        void TearDown() override {
            bmlUnloadAPI();
            if (m_Runtime) {
                bmlRuntimeDestroy(m_Runtime);
                m_Runtime = nullptr;
            }
            m_Services = nullptr;
        }

        BML_Mod LookupHostMod() const {
            auto *moduleApi = m_Services ? m_Services->Module : nullptr;
            if (!moduleApi || !moduleApi->Context || !moduleApi->FindModuleById) {
                return nullptr;
            }
            return moduleApi->FindModuleById(moduleApi->Context, "ModLoader");
        }

        static std::filesystem::path MakeTempTestDirectory(const char *prefix) {
            static std::atomic<uint32_t> counter{0};
            const uint32_t value = counter.fetch_add(1, std::memory_order_relaxed);
            auto path = std::filesystem::temp_directory_path() /
                (std::string(prefix) + "-" + std::to_string(value));
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
            std::filesystem::create_directories(path, ec);
            return path;
        }

        BML_Runtime m_Runtime{nullptr};
        const BML_Services *m_Services{nullptr};
    };

    struct TestServiceApi {
        int (*Add)(int a, int b);
    };

    int AddInts(int a, int b) {
        return a + b;
    }

    struct TestHandlerState {
        std::atomic<uint32_t> call_count{0};
        uint32_t last_payload{0};
    };

    void TestImcHandler(BML_Context,
                        BML_TopicId,
                        const BML_ImcMessage *msg,
                        void *user_data) {
        auto *state = static_cast<TestHandlerState *>(user_data);
        if (!state || !msg) {
            return;
        }
        if (msg->data && msg->size == sizeof(uint32_t)) {
            std::memcpy(&state->last_payload, msg->data, sizeof(uint32_t));
        }
        state->call_count.fetch_add(1, std::memory_order_relaxed);
    }
} // namespace

TEST_F(BMLIntegrationTest, RuntimeExposesServices) {
    ASSERT_NE(nullptr, m_Services->Context);
    ASSERT_NE(nullptr, m_Services->Module);
    ASSERT_NE(nullptr, m_Services->InterfaceControl);
    ASSERT_NE(nullptr, m_Services->ImcBus);
    ASSERT_NE(nullptr, m_Services->HostRuntime);
    EXPECT_NE(nullptr, m_Services->Context->Context);
}

TEST_F(BMLIntegrationTest, ServiceBundlesAreStablePerRuntime) {
    BML_Runtime otherRuntime = nullptr;
    BML_RuntimeConfig config = BML_RUNTIME_CONFIG_INIT;
    ASSERT_EQ(BML_RESULT_OK, bmlRuntimeCreate(&config, &otherRuntime));
    ASSERT_NE(nullptr, otherRuntime);

    const BML_Services *otherServices = bmlRuntimeGetServices(otherRuntime);
    ASSERT_NE(nullptr, otherServices);

    EXPECT_NE(m_Services, otherServices);
    EXPECT_NE(m_Services->Context, otherServices->Context);
    EXPECT_NE(m_Services->Module, otherServices->Module);
    EXPECT_NE(m_Services->ImcBus, otherServices->ImcBus);

    bmlRuntimeDestroy(otherRuntime);
}

TEST_F(BMLIntegrationTest, BootstrapLoaderStillLoadsHostBootstrapMinimum) {
    ASSERT_EQ(BML_RESULT_OK, BML_BOOTSTRAP_LOAD(bmlGetProcAddress));
    EXPECT_TRUE(bmlIsApiLoaded());
    EXPECT_NE(nullptr, bmlInterfaceRegister);
    EXPECT_NE(nullptr, bmlInterfaceAcquire);
    EXPECT_NE(nullptr, bmlInterfaceRelease);
    EXPECT_NE(nullptr, bmlInterfaceUnregister);
    EXPECT_NE(nullptr, bmlGetProcAddress("bmlGetProcAddress"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlGetLoadedModuleCount"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlTraceBegin"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlHookEnumerate"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcGetTopicId"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcGetRpcId"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcPump"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcGetStats"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcGetTopicInfo"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcCopyState"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcClearState"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcGetRpcInfo"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlImcEnumerateRpc"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlRegisterRuntimeProvider"));
    EXPECT_EQ(nullptr, bmlGetProcAddress("bmlCleanupModuleState"));
}

TEST_F(BMLIntegrationTest, ActiveCoreSourcesDoNotKeepRemovedZeroHandleShims) {
    const auto repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path();

    const auto memoryApi = ReadFileUtf8(repoRoot / "src" / "Core" / "MemoryApi.cpp");
    ASSERT_FALSE(memoryApi.empty());
    EXPECT_EQ(std::string::npos, memoryApi.find("BML_API_Alloc("));
    EXPECT_EQ(std::string::npos, memoryApi.find("BML_API_GetMemoryStats("));

    const auto profilingApi = ReadFileUtf8(repoRoot / "src" / "Core" / "ProfilingApi.cpp");
    ASSERT_FALSE(profilingApi.empty());
    EXPECT_EQ(std::string::npos, profilingApi.find("BML_API_TraceBegin("));
    EXPECT_EQ(std::string::npos, profilingApi.find("BML_API_GetApiCallCount("));

    const auto syncApi = ReadFileUtf8(repoRoot / "src" / "Core" / "SyncApi.cpp");
    ASSERT_FALSE(syncApi.empty());
    EXPECT_EQ(std::string::npos, syncApi.find("BML_API_MutexCreate("));
    EXPECT_EQ(std::string::npos, syncApi.find("BML_API_SemaphoreCreate("));

    const auto tracingApi = ReadFileUtf8(repoRoot / "src" / "Core" / "TracingApi.cpp");
    ASSERT_FALSE(tracingApi.empty());
    EXPECT_EQ(std::string::npos, tracingApi.find("BML_EnableApiTracing("));
    EXPECT_EQ(std::string::npos, tracingApi.find("BML_GetApiStats("));

    const auto localeApi = ReadFileUtf8(repoRoot / "src" / "Core" / "LocaleApi.cpp");
    ASSERT_FALSE(localeApi.empty());
    EXPECT_EQ(std::string::npos, localeApi.find("BML_API_LocaleSetLanguage("));
    EXPECT_EQ(std::string::npos, localeApi.find("BML_API_LocaleGetLanguage("));
    EXPECT_EQ(std::string::npos, localeApi.find("BML_API_LocaleLookup("));

    const auto hookApi = ReadFileUtf8(repoRoot / "src" / "Core" / "HookApi.cpp");
    ASSERT_FALSE(hookApi.empty());
    EXPECT_EQ(std::string::npos, hookApi.find("BML_API_HookEnumerate("));
    EXPECT_EQ(std::string::npos, hookApi.find("bmlHookEnumerate"));

    const auto coreApi = ReadFileUtf8(repoRoot / "src" / "Core" / "CoreApi.cpp");
    ASSERT_FALSE(coreApi.empty());
    EXPECT_EQ(std::string::npos, coreApi.find("bmlRegisterRuntimeProvider"));
    EXPECT_EQ(std::string::npos, coreApi.find("bmlUnregisterRuntimeProvider"));
    EXPECT_EQ(std::string::npos, coreApi.find("bmlCleanupModuleState"));

    const auto imcApi = ReadFileUtf8(repoRoot / "src" / "Core" / "ImcBusApi.cpp");
    ASSERT_FALSE(imcApi.empty());
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcGetTopicId"));
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcGetRpcId"));
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcPump"));
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcGetStats"));
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcGetTopicInfo"));
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcCopyState"));
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcClearState"));
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcGetRpcInfo"));
    EXPECT_EQ(std::string::npos, imcApi.find("bmlImcEnumerateRpc"));
}

TEST_F(BMLIntegrationTest, ActiveCoreSourcesDoNotDependOnAmbientServices) {
    const auto repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path();

    const auto kernelServices = ReadFileUtf8(repoRoot / "src" / "Core" / "KernelServices.h");
    ASSERT_FALSE(kernelServices.empty());
    EXPECT_EQ(std::string::npos, kernelServices.find("KernelServices &GetActiveServices() noexcept;"));
    EXPECT_EQ(std::string::npos, kernelServices.find("KernelServices *TryGetActiveServices() noexcept;"));

    const auto runtimeInterfaces = ReadFileUtf8(repoRoot / "src" / "Core" / "RuntimeInterfaces.cpp");
    ASSERT_FALSE(runtimeInterfaces.empty());
    EXPECT_EQ(std::string::npos, runtimeInterfaces.find("GetActiveServices("));
    EXPECT_EQ(std::string::npos, runtimeInterfaces.find("ActiveServicesScope"));

    const auto microkernel = ReadFileUtf8(repoRoot / "src" / "Core" / "Microkernel.cpp");
    ASSERT_FALSE(microkernel.empty());
    EXPECT_EQ(std::string::npos, microkernel.find("GetActiveServices("));
    EXPECT_EQ(std::string::npos, microkernel.find("ActiveServicesScope"));

    const auto moduleRuntime = ReadFileUtf8(repoRoot / "src" / "Core" / "ModuleRuntime.cpp");
    ASSERT_FALSE(moduleRuntime.empty());
    EXPECT_EQ(std::string::npos, moduleRuntime.find("GetActiveServices("));

    const auto logging = ReadFileUtf8(repoRoot / "src" / "Core" / "Logging.cpp");
    ASSERT_FALSE(logging.empty());
    EXPECT_EQ(std::string::npos, logging.find("GetActiveServices("));

    const auto exportApi = ReadFileUtf8(repoRoot / "src" / "Core" / "Export.cpp");
    ASSERT_FALSE(exportApi.empty());
    EXPECT_EQ(std::string::npos, exportApi.find("ActiveServicesScope"));

    const auto registrationMacros =
        ReadFileUtf8(repoRoot / "src" / "Core" / "ApiRegistrationMacros.h");
    ASSERT_FALSE(registrationMacros.empty());
    EXPECT_EQ(std::string::npos, registrationMacros.find("GetActiveServices("));

    const auto imcBus = ReadFileUtf8(repoRoot / "src" / "Core" / "ImcBus.cpp");
    ASSERT_FALSE(imcBus.empty());
    EXPECT_EQ(std::string::npos, imcBus.find("GetActiveServices("));

    const auto imcFacade = ReadFileUtf8(repoRoot / "src" / "Core" / "ImcBusFacade.cpp");
    ASSERT_FALSE(imcFacade.empty());
    EXPECT_EQ(std::string::npos, imcFacade.find("GetActiveServices("));
    EXPECT_EQ(std::string::npos, imcFacade.find("GetInstalledTestKernel("));

    const auto testKernelHeader = ReadFileUtf8(repoRoot / "tests" / "TestKernel.h");
    ASSERT_FALSE(testKernelHeader.empty());
    EXPECT_EQ(std::string::npos, testKernelHeader.find("GetInstalledTestKernel("));

    const auto exportStub = ReadFileUtf8(repoRoot / "tests" / "ExportStub.cpp");
    ASSERT_FALSE(exportStub.empty());
    EXPECT_EQ(std::string::npos, exportStub.find("GetInstalledTestKernel("));
}

TEST_F(BMLIntegrationTest, PublicHeadersAndDesignDocsDoNotExposeRemovedLegacyContracts) {
    const auto repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path();

    const auto errorsHeader = ReadFileUtf8(repoRoot / "include" / "bml_errors.h");
    ASSERT_FALSE(errorsHeader.empty());
    EXPECT_EQ(std::string::npos, errorsHeader.find("PFN_BML_GetLastError"));
    EXPECT_EQ(std::string::npos, errorsHeader.find("PFN_BML_ClearLastError"));

    const auto contextHeader = ReadFileUtf8(repoRoot / "include" / "bml_context.hpp");
    ASSERT_FALSE(contextHeader.empty());
    EXPECT_EQ(std::string::npos, contextHeader.find("GetGlobalContext("));

    const auto scriptingDesign =
        ReadFileUtf8(repoRoot / "docs" / "design" / "embedded-scripting-angelscript.md");
    ASSERT_FALSE(scriptingDesign.empty());
    EXPECT_EQ(std::string::npos, scriptingDesign.find("bmlRegisterRuntimeProvider"));
    EXPECT_EQ(std::string::npos, scriptingDesign.find("bmlUnregisterRuntimeProvider"));
    EXPECT_EQ(std::string::npos, scriptingDesign.find("bmlCleanupModuleState"));
    EXPECT_EQ(std::string::npos, scriptingDesign.find("PFN_BML_GetProcAddress"));

    const auto dimensionsDesign =
        ReadFileUtf8(repoRoot / "docs" / "design" / "missing-dimensions-analysis.md");
    ASSERT_FALSE(dimensionsDesign.empty());
    EXPECT_EQ(std::string::npos, dimensionsDesign.find("PFN_BML_HookEnumerate"));

    const auto servicesHeader = ReadFileUtf8(repoRoot / "include" / "bml_services.hpp");
    ASSERT_FALSE(servicesHeader.empty());
    EXPECT_EQ(std::string::npos, servicesHeader.find("BuiltinServices"));
    EXPECT_EQ(std::string::npos, servicesHeader.find("Builtins("));
    EXPECT_FALSE(std::filesystem::exists(repoRoot / "include" / "bml_builtin_interfaces.h"));

    const auto runtimeInterfacesSource =
        ReadFileUtf8(repoRoot / "src" / "Core" / "RuntimeInterfaces.cpp");
    ASSERT_FALSE(runtimeInterfacesSource.empty());
    EXPECT_EQ(std::string::npos, runtimeInterfacesSource.find("BML_API_Builtin"));
}

TEST_F(BMLIntegrationTest, InterfaceRegisterAcquireCycleUsesBoundServices) {
    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);
    ASSERT_NE(nullptr, m_Services->InterfaceControl);

    static TestServiceApi service{&AddInts};
    BML_InterfaceDesc desc = BML_INTERFACE_DESC_INIT;
    desc.interface_id = "test.integration.clean_break.service";
    desc.abi_version = bmlMakeVersion(1, 0, 0);
    desc.implementation = &service;
    desc.implementation_size = sizeof(service);

    ASSERT_EQ(BML_RESULT_OK, m_Services->InterfaceControl->Register(hostMod, &desc));

    const void *implementation = nullptr;
    BML_InterfaceLease lease = nullptr;
    BML_Version req = bmlMakeVersion(1, 0, 0);
    ASSERT_EQ(BML_RESULT_OK,
              m_Services->InterfaceControl->Acquire(
                  hostMod,
                  desc.interface_id,
                  &req,
                  &implementation,
                  &lease));
    ASSERT_NE(nullptr, implementation);
    ASSERT_NE(nullptr, lease);

    auto *loaded = static_cast<const TestServiceApi *>(implementation);
    EXPECT_EQ(5, loaded->Add(2, 3));

    EXPECT_EQ(BML_RESULT_BUSY,
              m_Services->InterfaceControl->Unregister(hostMod, desc.interface_id));
    EXPECT_EQ(BML_RESULT_OK, m_Services->InterfaceControl->Release(lease));
    EXPECT_EQ(BML_RESULT_OK,
              m_Services->InterfaceControl->Unregister(hostMod, desc.interface_id));
}

TEST_F(BMLIntegrationTest, FullImcPublishSubscribeCycleUsesServices) {
    BML_Mod hostMod = LookupHostMod();
    ASSERT_NE(nullptr, hostMod);
    ASSERT_NE(nullptr, m_Services->ImcBus);

    BML_TopicId topic = BML_TOPIC_ID_INVALID;
    ASSERT_EQ(BML_RESULT_OK,
              m_Services->ImcBus->GetTopicId(
                  m_Services->ImcBus->Context, "test/integration/pubsub", &topic));
    ASSERT_NE(BML_TOPIC_ID_INVALID, topic);

    TestHandlerState state;
    BML_Subscription sub = nullptr;
    ASSERT_EQ(BML_RESULT_OK,
              m_Services->ImcBus->Subscribe(hostMod, topic, TestImcHandler, &state, &sub));
    ASSERT_NE(nullptr, sub);

    uint32_t payload = 42;
    ASSERT_EQ(BML_RESULT_OK,
              m_Services->ImcBus->Publish(hostMod, topic, &payload, sizeof(payload)));
    m_Services->ImcBus->Pump(m_Services->ImcBus->Context, 100);

    EXPECT_EQ(1u, state.call_count.load());
    EXPECT_EQ(42u, state.last_payload);

    ASSERT_EQ(BML_RESULT_OK, m_Services->ImcBus->Unsubscribe(sub));
}

TEST_F(BMLIntegrationTest, ForgedInterfaceLeaseFailsGracefully) {
    ASSERT_NE(nullptr, m_Services->InterfaceControl);
    auto fakeLease = reinterpret_cast<BML_InterfaceLease>(static_cast<uintptr_t>(0x1234));
    EXPECT_EQ(BML_RESULT_INVALID_HANDLE, m_Services->InterfaceControl->Release(fakeLease));
}

TEST_F(BMLIntegrationTest, PartialModuleLoadPopulatesDiagnostics) {
    namespace fs = std::filesystem;

    const fs::path tempDir = MakeTempTestDirectory("bml-clean-break-partial");
    const fs::path modDir = tempDir / "BrokenNativeMod";
    std::error_code ec;
    ASSERT_TRUE(fs::create_directories(modDir, ec));
    ASSERT_FALSE(ec);

    {
        std::ofstream manifest(modDir / "mod.toml", std::ios::binary);
        ASSERT_TRUE(manifest.is_open());
        manifest << "[package]\n";
        manifest << "id = \"broken.native\"\n";
        manifest << "name = \"Broken Native\"\n";
        manifest << "version = \"1.0.0\"\n";
        manifest << "entry = \"../escape-native.dll\"\n";
    }

    bmlUnloadAPI();
    bmlRuntimeDestroy(m_Runtime);
    m_Runtime = nullptr;
    m_Services = nullptr;

    BML_RuntimeConfig config = BML_RUNTIME_CONFIG_INIT;
    const std::string tempDirUtf8 = tempDir.generic_string();
    config.mods_dir_utf8 = tempDirUtf8.c_str();

    ASSERT_EQ(BML_RESULT_OK, bmlRuntimeCreate(&config, &m_Runtime));
    ASSERT_NE(nullptr, m_Runtime);
    m_Services = bmlRuntimeGetServices(m_Runtime);
    ASSERT_NE(nullptr, m_Services);
    bmlBindServices(m_Services);

    ASSERT_EQ(BML_RESULT_OK, bmlRuntimeDiscoverModules(m_Runtime));
    ASSERT_EQ(BML_RESULT_OK, bmlRuntimeLoadModules(m_Runtime));

    const BML_BootstrapDiagnostics *diag = bmlRuntimeGetBootstrapDiagnostics(m_Runtime);
    ASSERT_NE(nullptr, diag);
    EXPECT_NE(0, diag->load_error.has_error);
    ASSERT_NE(nullptr, diag->load_error.module_id);
    EXPECT_STREQ("broken.native", diag->load_error.module_id);

    fs::remove_all(tempDir, ec);
}
