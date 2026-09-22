#ifndef BML_TESTS_IVP_RETAIL_TEST_SUPPORT_H
#define BML_TESTS_IVP_RETAIL_TEST_SUPPORT_H

#include "BML/IVP/Environment.h"
#include "BML/IVP/Actuator.h"
#include "BML/IVP/ActiveValue.h"
#include "BML/IVP/Constraint.h"
#include "BML/IVP/CollisionFilter.h"
#include "BML/IVP/CollisionSolver.h"
#include "BML/IVP/GreatMatrix.h"
#include "BML/IVP/Grid.h"
#include "BML/IVP/Buoyancy.h"
#include "BML/IVP/Broadphase.h"
#include "BML/IVP/Debug.h"
#include "BML/IVP/Material.h"
#include "BML/IVP/Performance.h"
#include "BML/IVP/Anomaly.h"
#include "BML/IVP/SurfaceBuilder.h"
#include "BML/IVP/Templates.h"
#include "BML/IVP/StringHash.h"
#include "BML/IVP/MinHash.h"
#include "BML/IVP/Set.h"
#include "CryptoUtils.h"
#include "IvpTestAdapter.h"

#include <gtest/gtest.h>

#include <Windows.h>
#include <float.h>

#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <new>
#include <memory>
#include <string>
#include <vector>

extern std::uintptr_t gRetailModuleBase;

namespace {

struct RawCacheObjectPrefix {
    IVP_Time_CODE validUntilTimeCode;
    std::int32_t referenceCount;
    IVP_Real_Object *object;
    std::uint32_t reserved;
};

struct RawCacheManager {
    std::int32_t cacheObjectCount;
    std::int32_t reuseLoopIndex;
    char *cacheObjectsBuffer;
};

struct RawOVTreeManagerLayout {
    IVP_DOUBLE powerlist[81];
    std::array<std::byte, 0x28> searchNode;
    IVP_U_Vector<IVP_OV_Element> *collisionPartners;
    IVP_ov_tree_hash *hashTable;
    IVP_Environment *environment;
    IVP_OV_Node *root;
};

static_assert(sizeof(RawCacheObjectPrefix) == 0x10);
static_assert(sizeof(RawCacheManager) == 0x0C);
static_assert(sizeof(RawOVTreeManagerLayout) == 0x2C0);
static_assert(offsetof(RawOVTreeManagerLayout, root) == 0x2BC);

RawCacheObjectPrefix &RawCache(IVP_Cache_Object *cache) {
    return *reinterpret_cast<RawCacheObjectPrefix *>(cache);
}

RawCacheManager &RawCacheManagerState(IVP_Cache_Object_Manager *manager) {
    return *reinterpret_cast<RawCacheManager *>(manager);
}

RawOVTreeManagerLayout &RawOVTreeManager(IVP_OV_Tree_Manager *manager) {
    return *reinterpret_cast<RawOVTreeManagerLayout *>(manager);
}

class TestCollision final : public IVP_Collision {
public:
    TestCollision() : IVP_Collision(nullptr) {}

    void simulate_time_event(IVP_Environment *) override {}
    void get_objects(IVP_Real_Object *objectsOut[2]) override {
        objectsOut[0] = nullptr;
        objectsOut[1] = nullptr;
    }
    void get_ledges(const IVP_Compact_Ledge *ledgesOut[2]) override {
        ledgesOut[0] = nullptr;
        ledgesOut[1] = nullptr;
    }
};

class RecordingGlobalObjectListener final : public IVP_Listener_Object {
public:
    RecordingGlobalObjectListener(
        char identity, IVP_Environment *environment,
        std::vector<std::string> *events, IVP_Event_Object *expectedEvent,
        bool removeWhenRevived = false)
        : identity_(identity), environment_(environment), events_(events),
          expected_event_(expectedEvent),
          remove_when_revived_(removeWhenRevived) {}

    void event_object_deleted(IVP_Event_Object *event) override {
        record(event, 'D');
    }
    void event_object_created(IVP_Event_Object *event) override {
        record(event, 'C');
    }
    void event_object_revived(IVP_Event_Object *event) override {
        record(event, 'R');
        if (remove_when_revived_)
            environment_->remove_listener_object_global(this);
    }
    void event_object_frozen(IVP_Event_Object *event) override {
        record(event, 'F');
    }

private:
    void record(IVP_Event_Object *event, char kind) {
        EXPECT_EQ(event, expected_event_);
        events_->emplace_back(std::string{kind, identity_});
    }

    char identity_;
    IVP_Environment *environment_;
    std::vector<std::string> *events_;
    IVP_Event_Object *expected_event_;
    bool remove_when_revived_;
};

class RecordingCollisionDelegatorRoot final
    : public IVP_Collision_Delegator_Root {
public:
    RecordingCollisionDelegatorRoot(
        char identity, std::vector<char> *events,
        IVP_Real_Object *expectedObject)
        : identity_(identity), events_(events),
          expected_object_(expectedObject) {}

    void collision_is_going_to_be_deleted_event(IVP_Collision *) override {}
    void object_is_removed_from_collision_detection(
        IVP_Real_Object *object) override {
        EXPECT_EQ(object, expected_object_);
        events_->push_back(identity_);
    }
    IVP_Collision *delegate_collisions_for_object(
        IVP_Real_Object *, IVP_Real_Object *) override {
        return nullptr;
    }
    void environment_is_going_to_be_deleted_event(
        IVP_Environment *) override {}

private:
    char identity_;
    std::vector<char> *events_;
    IVP_Real_Object *expected_object_;
};


}

namespace {

constexpr std::uint32_t kExpectedTimestamp = 0x3DAC380Cu;
constexpr std::uint32_t kExpectedImageSize = 0x00081000u;
constexpr char kExpectedSha256[] =
    "e72e4afcfa5c33a7d3d27776137f8c997b3c52d89d8a8a4745f1ca21e45893ec";
constexpr std::uint32_t kPostCollisionDispatcherRva = 0x00013B80u;
constexpr std::uint32_t kFrictionCreatedDispatcherRva = 0x00013BC0u;
constexpr std::uint32_t kFrictionDeletedDispatcherRva = 0x00013C00u;
constexpr std::uint32_t kAddCollisionListenerRva = 0x00013C70u;
constexpr std::uint32_t kIncrementalLuDecomposeRva = 0x00035AE0u;
constexpr std::uint32_t kIncrementalLuDecrementRva = 0x00035D00u;
constexpr std::uint32_t kRealObjectGetCacheObjectInlineRva = 0x0001A190u;
constexpr std::uint32_t kVectorAllocatedConstructorInlineRva = 0x00024C20u;
constexpr std::uint32_t kActiveFloatAddDependencyInlineRva = 0x00015BA0u;
constexpr std::uint32_t kActiveFloatRemoveDependencyInlineRva = 0x00015BE0u;

class RecordingCollisionFilter final : public IVP_Collision_Filter {
public:
    explicit RecordingCollisionFilter(IVP_BOOL result) : result_(result) {}

    IVP_BOOL check_objects_for_collision_detection(
        IVP_Real_Object *, IVP_Real_Object *) override {
        ++checkCalls;
        return result_;
    }
    void environment_will_be_deleted(IVP_Environment *environment) override {
        lastEnvironment = environment;
        ++environmentDeleteCalls;
    }

    int checkCalls = 0;
    int environmentDeleteCalls = 0;
    IVP_Environment *lastEnvironment = nullptr;

private:
    IVP_BOOL result_;
};

class ScopedDllDirectory {
public:
    explicit ScopedDllDirectory(const std::filesystem::path &directory) {
        valid_ = SetDllDirectoryW(directory.c_str()) != FALSE;
    }

    ~ScopedDllDirectory() {
        if (valid_)
            SetDllDirectoryW(nullptr);
    }

    bool valid() const { return valid_; }

private:
    bool valid_ = false;
};

class ScopedModule {
public:
    explicit ScopedModule(const std::filesystem::path &path)
        : module_(LoadLibraryW(path.c_str())) {}

    // physics_RT owns process-global state and is never unloaded during a
    // Ballance run. Keep the exact image resident across test cases as well;
    // unload/reload in one process corrupts its old CRT/global state.
    ~ScopedModule() = default;

    HMODULE get() const { return module_; }

private:
    HMODULE module_ = nullptr;
};

std::wstring RetailDllPath() {
    const DWORD required = GetEnvironmentVariableW(
        L"BML_TEST_RETAIL_PHYSICS_DLL", nullptr, 0);
    if (required == 0)
        return {};

    std::wstring value(required, L'\0');
    const DWORD written = GetEnvironmentVariableW(
        L"BML_TEST_RETAIL_PHYSICS_DLL", value.data(), required);
    if (written == 0 || written >= required)
        return {};
    value.resize(written);
    return value;
}

::testing::AssertionResult VerifyRetailImage(HMODULE module) {
    const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return ::testing::AssertionFailure() << "Invalid DOS signature";
    const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS32 *>(
        reinterpret_cast<const std::byte *>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)
        return ::testing::AssertionFailure() << "Invalid PE signature";
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386)
        return ::testing::AssertionFailure() << "Retail image is not x86";
    if (nt->FileHeader.TimeDateStamp != kExpectedTimestamp)
        return ::testing::AssertionFailure() << "Unexpected PE timestamp";
    if (nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        return ::testing::AssertionFailure() << "Unexpected PE format";
    if (nt->OptionalHeader.SizeOfImage != kExpectedImageSize)
        return ::testing::AssertionFailure() << "Unexpected image size";
    return ::testing::AssertionSuccess();
}

class RetailImage {
public:
    RetailImage() {
        gRetailModuleBase = 0;
        const std::wstring configuredPath = RetailDllPath();
        if (configuredPath.empty())
            return;
        configured_ = true;
        path_ = configuredPath;

        std::error_code error;
        if (!std::filesystem::is_regular_file(path_, error)) {
            error_ = "Retail physics DLL is missing: " + path_.string();
            return;
        }
        if (utils::Sha256FileHex(path_.wstring()) != kExpectedSha256) {
            error_ = "Retail physics DLL SHA-256 does not match Ballance";
            return;
        }

        search_path_ = std::make_unique<ScopedDllDirectory>(
            path_.parent_path().parent_path() / L"Bin");
        if (!search_path_->valid()) {
            error_ = "Cannot set the Ballance DLL directory: " +
                     std::to_string(GetLastError());
            return;
        }
        module_ = std::make_unique<ScopedModule>(path_);
        if (!module_->get()) {
            error_ = "Cannot load retail physics DLL: " +
                     std::to_string(GetLastError());
            return;
        }
        const auto verified = VerifyRetailImage(module_->get());
        if (!verified) {
            error_ = verified.message();
            return;
        }
        gRetailModuleBase = reinterpret_cast<uintptr_t>(module_->get());
    }

    ~RetailImage() { gRetailModuleBase = 0; }

    RetailImage(const RetailImage &) = delete;
    RetailImage &operator=(const RetailImage &) = delete;

    bool configured() const { return configured_; }
    bool valid() const { return get() != nullptr && error_.empty(); }
    const std::string &error() const { return error_; }
    HMODULE get() const { return module_ ? module_->get() : nullptr; }

private:
    bool configured_ = false;
    std::filesystem::path path_;
    std::string error_;
    std::unique_ptr<ScopedDllDirectory> search_path_;
    std::unique_ptr<ScopedModule> module_;
};

} // namespace

#endif // BML_TESTS_IVP_RETAIL_TEST_SUPPORT_H
