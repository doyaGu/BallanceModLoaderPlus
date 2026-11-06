#include "BML/Mod.hpp"
#include <gtest/gtest.h>
#include <stdexcept>

namespace {
int factories = 0, constructors = 0, destructors = 0, processes = 0, starts = 0;
bool failFactory = false, failConstructor = false, failProcess = false;

class SampleMod final : public BML::Mod::Instance {
  public:
    SampleMod() {
        if (failConstructor)
            throw std::bad_alloc();
        ++constructors;
    }
    ~SampleMod() noexcept override { ++destructors; }
    void OnProcess(const BML::Mod::Context &ctx) override {
        EXPECT_TRUE(ctx.IsValid());
        if (failProcess)
            throw std::runtime_error("business failure");
        ++processes;
    }
    void OnStartLevel(const BML::Mod::Context &) override { ++starts; }
};
BML::Mod::Definition Define() {
    ++factories;
    if (failFactory)
        throw std::bad_alloc();
    return {"short",
            "SSO",
            "1.0.0",
            "author",
            "description longer than the usual short string buffer",
            "",
            {{"base", "1.2.3", false}, {"optional", "", true}}};
}
int BML_CDECL Owner(BML_ModContextHandle h, BML_ModHandle *out) {
    if (h != 123)
        return BML_ERROR_INVALID_HANDLE;
    *out = 42;
    return BML_OK;
}
const BML_ModContextInterface host{
    BML_IFACE_HEADER(BML_ModContextInterface, BML_MOD_CONTEXT_INTERFACE_ID, 1, 0),
    Owner,
    BML::Detail::Cdecl<[](BML_ModContextHandle, BML_ModHandle, BML_ModVisitor, void *) {
        return BML_ERROR_NOT_IMPLEMENTED;
    }>::Call,
    BML::Detail::Cdecl<[](BML_ModContextHandle, const char *, BML_ModHandle *) {
        return BML_ERROR_NOT_IMPLEMENTED;
    }>::Call,
    BML::Detail::Cdecl<[](BML_ModContextHandle, BML_ModVisitor, void *) {
        return BML_ERROR_NOT_IMPLEMENTED;
    }>::Call,
    BML::Detail::Cdecl<[](BML_ModContextHandle, uint32_t, const char *, size_t) {
        return BML_OK;
    }>::Call,
    BML::Detail::Cdecl<[](BML_ModContextHandle, uint32_t, char *, size_t, size_t *) {
        return BML_ERROR_NOT_IMPLEMENTED;
    }>::Call,
    BML::Detail::Cdecl<[](BML_ModContextHandle, const char *, char *, size_t, size_t *) {
        return BML_ERROR_NOT_IMPLEMENTED;
    }>::Call,
    BML::Detail::Cdecl<[](BML_ModContextHandle, const char *, void *, size_t, size_t *) {
        return BML_ERROR_NOT_IMPLEMENTED;
    }>::Call,
    BML::Detail::Cdecl<[](BML_ModContextHandle, const char *, uint32_t *) {
        return BML_ERROR_NOT_IMPLEMENTED;
    }>::Call};
} // namespace

int BML_CDECL BML_GetInterface(const char *id, uint16_t major, const void **out) {
    *out = nullptr;
    if (std::strcmp(id, BML_MOD_CONTEXT_INTERFACE_ID))
        return BML_ERROR_NOT_FOUND;
    if (major != 1)
        return BML_ERROR_VERSION_MISMATCH;
    *out = &host;
    return BML_OK;
}
BML_DEFINE_MOD(SampleMod, Define)

TEST(ModProtocol, QueryMetadataAndModuleLocalTrampolines) {
    const BML_ModInterface *api = reinterpret_cast<const BML_ModInterface *>(1);
    EXPECT_EQ(BML_GetModInterface(2, &api), BML_ERROR_VERSION_MISMATCH);
    EXPECT_EQ(api, nullptr);
    EXPECT_EQ(factories, 0);
    EXPECT_EQ(BML_GetModInterface(1, nullptr), BML_ERROR_INVALID_PARAMETER);

    failFactory = true;
    EXPECT_EQ(BML_GetModInterface(1, &api), BML_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(api, nullptr);
    EXPECT_EQ(constructors, 0);
    failFactory = false;
    ASSERT_EQ(BML_GetModInterface(1, &api), BML_OK);
    ASSERT_NE(api, nullptr);
    const auto *first = api;
    ASSERT_EQ(BML_GetModInterface(1, &api), BML_OK);
    EXPECT_EQ(api, first);
    EXPECT_EQ(factories, 2);
    EXPECT_EQ(constructors, 0);
    EXPECT_STREQ(api->Definition->Id, "short");
    EXPECT_STREQ(api->Definition->Name, "SSO");
    EXPECT_STREQ(api->Definition->Dependencies[0].Id, "base");
    EXPECT_STREQ(api->Definition->Dependencies[0].MinimumVersion, "1.2.3");
    EXPECT_EQ(api->Definition->Dependencies[1].Optional, 1u);
    EXPECT_EQ(api->Callbacks->OnLoad, nullptr);
    EXPECT_EQ(api->Callbacks->OnRender, nullptr);
    EXPECT_NE(api->Callbacks->OnProcess, nullptr);
    EXPECT_NE(api->Callbacks->OnGameEvent, nullptr);
    EXPECT_EQ(api->Callbacks->OnCommandEvent, nullptr);

    void *instance = reinterpret_cast<void *>(1);
    failConstructor = true;
    EXPECT_EQ(api->Create(123, &instance), BML_ERROR_OUT_OF_MEMORY);
    EXPECT_EQ(instance, nullptr);
    EXPECT_EQ(destructors, 0);
    failConstructor = false;
    ASSERT_EQ(api->Create(123, &instance), BML_OK);
    ASSERT_NE(instance, nullptr);
    EXPECT_EQ(constructors, 1);
    EXPECT_EQ(api->Callbacks->OnProcess(instance, 123), BML_OK);
    EXPECT_EQ(processes, 1);
    failProcess = true;
    EXPECT_EQ(api->Callbacks->OnProcess(instance, 123), BML_ERROR_FAIL);
    failProcess = false;
    EXPECT_EQ(api->Callbacks->OnGameEvent(instance, 123, BML_GAME_EVENT_START_LEVEL), BML_OK);
    EXPECT_EQ(starts, 1);
    EXPECT_EQ(api->Callbacks->OnGameEvent(instance, 123, UINT32_MAX), BML_OK);
    EXPECT_EQ(starts, 1);
    api->Destroy(instance);
    EXPECT_EQ(destructors, 1);
}

TEST(ModProtocol, BufferGrowthHasBoundedRetriesAndNoPartialResult) {
    int reads = 0;
    auto result = BML::Detail::ReadBuffer<std::string>([&](char *, size_t, size_t *required) {
        *required = static_cast<size_t>(++reads);
        return BML_ERROR_BUFFER_TOO_SMALL;
    });
    EXPECT_EQ(reads, 3); // measure, initial read, one retry
    EXPECT_EQ(result.Code(), BML_ERROR_BUFFER_TOO_SMALL);
    EXPECT_FALSE(result.HasValue());
    auto empty =
        BML::Detail::ReadBuffer<std::string>([](char *buffer, size_t size, size_t *required) {
            *required = 1;
            if (!size)
                return BML_ERROR_BUFFER_TOO_SMALL;
            *buffer = 0;
            return BML_OK;
        });
    ASSERT_TRUE(empty);
    EXPECT_TRUE(empty->empty());
}

TEST(ModProtocol, BorrowedEventConversionDoesNotOwnOrCopyPayload) {
    struct Receiver : BML::Mod::Instance {
        const char *Expected = nullptr;
        void OnLoadObject(const BML::Mod::Context &,
                          const BML::Mod::LoadObjectEvent &event) override {
            EXPECT_EQ(event.Filename.data(), Expected);
            EXPECT_EQ(event.Objects.size(), 1u);
        }
    } receiver;
    const char filename[] = "Unicode-地图.nmo";
    receiver.Expected = filename;
    const BML_ObjectRef object{1, 2, 3};
    BML_LoadObjectEvent event{};
    event.StructSize = sizeof(event);
    event.Filename = filename;
    event.Objects = &object;
    event.ObjectCount = 1;
    EXPECT_EQ(BML::Mod::Detail::Export<Receiver>::OnLoadObject(&receiver, 123, &event), BML_OK);
    event.StructSize = sizeof(size_t);
    EXPECT_EQ(BML::Mod::Detail::Export<Receiver>::OnLoadObject(&receiver, 123, &event),
              BML_ERROR_INVALID_PARAMETER);
}
