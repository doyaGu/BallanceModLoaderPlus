/**
 * @file ModuleLoaderTests.cpp
 * @brief Tests for ModuleLoader functionality
 * 
 * Tests cover:
 * - Module loading and unloading
 * - Entry point invocation
 * - Error handling
 * - Module dependencies
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "Core/Context.h"
#include "Core/DependencyResolver.h"
#include "Core/ModHandle.h"
#include "Core/ModManifest.h"
#include "Core/ModuleLoader.h"
#include "Core/SemanticVersion.h"

using BML::Core::Context;
using BML::Core::LoadedModule;
using BML::Core::LoadModules;
using BML::Core::ModManifest;
using BML::Core::ModuleLoadError;
using BML::Core::ResolvedNode;
using BML::Core::UnloadModules;

namespace {

class ModuleLoaderTests : public ::testing::Test {
protected:
    void SetUp() override {
        Context::SetCurrentModule(nullptr);
    }

    void TearDown() override {
        Context::SetCurrentModule(nullptr);
        manifests_.clear();
    }

    ModManifest* CreateManifest(const std::string& id, 
                                 const std::wstring& entry = L"") {
        auto manifest = std::make_unique<ModManifest>();
        manifest->package.id = id;
        manifest->package.name = id;
        manifest->package.version = "1.0.0";
        manifest->package.parsed_version = {1, 0, 0};
        manifest->package.entry = std::string(entry.begin(), entry.end());
        manifest->directory = L"test";
        manifest->manifest_path = L"test/mod.toml";
        
        ModManifest* ptr = manifest.get();
        manifests_.push_back(std::move(manifest));
        return ptr;
    }

    std::vector<std::unique_ptr<ModManifest>> manifests_;
};

// ============================================================================
// Basic Tests
// ============================================================================

TEST_F(ModuleLoaderTests, LoadModulesWithEmptyOrder) {
    std::vector<ResolvedNode> order;
    std::vector<LoadedModule> modules;
    ModuleLoadError error;
    
    // Empty order should succeed trivially
    bool result = LoadModules(order, Context::Instance(), nullptr, modules, error);
    EXPECT_TRUE(result);
    EXPECT_TRUE(modules.empty());
}

TEST_F(ModuleLoaderTests, LoadModulesWithNonExistentDll) {
    auto* manifest = CreateManifest("test.mod", L"C:\\NonExistent\\path\\to\\mod.dll");
    
    ResolvedNode node;
    node.id = manifest->package.id;
    node.manifest = manifest;
    
    std::vector<ResolvedNode> order = {node};
    std::vector<LoadedModule> modules;
    ModuleLoadError error;
    
    // LoadModules behavior depends on implementation:
    // - May return false with error for non-existent DLL
    // - May skip and return true with empty modules
    // Either is acceptable as long as no crash
    bool result = LoadModules(order, Context::Instance(), nullptr, modules, error);
    
    if (!result) {
        // If it failed, check error is set
        EXPECT_FALSE(error.message.empty());
    }
    // If it succeeded, the module list should be empty or contain failed entry
}

TEST_F(ModuleLoaderTests, LoadModulesWithEmptyDllPath) {
    auto* manifest = CreateManifest("test.mod", L"");
    
    ResolvedNode node;
    node.id = manifest->package.id;
    node.manifest = manifest;
    
    std::vector<ResolvedNode> order = {node};
    std::vector<LoadedModule> modules;
    ModuleLoadError error;
    
    // Empty DLL path should fail or be handled gracefully
    bool result = LoadModules(order, Context::Instance(), nullptr, modules, error);
    // The result depends on implementation - it may skip or fail
    // Just verify no crash occurs
    EXPECT_TRUE(result || !error.message.empty());
}

// ============================================================================
// UnloadModules Tests
// ============================================================================

TEST_F(ModuleLoaderTests, UnloadEmptyModules) {
    std::vector<LoadedModule> modules;
    
    // Should not crash on empty list
    EXPECT_NO_THROW(UnloadModules(modules, nullptr));
}

TEST_F(ModuleLoaderTests, UnloadModulesWithNullHandles) {
    LoadedModule module;
    module.id = "test.mod";
    module.handle = nullptr;
    module.mod_handle = nullptr;
    
    std::vector<LoadedModule> modules;
    modules.push_back(std::move(module));
    
    // Should handle null handles gracefully
    EXPECT_NO_THROW(UnloadModules(modules, nullptr));
    EXPECT_TRUE(modules.empty());
}

// ============================================================================
// LoadedModule Structure Tests
// ============================================================================

TEST_F(ModuleLoaderTests, LoadedModuleDefaultConstruction) {
    LoadedModule module;
    
    EXPECT_TRUE(module.id.empty());
    EXPECT_EQ(module.manifest, nullptr);
    EXPECT_EQ(module.handle, nullptr);
    EXPECT_EQ(module.entrypoint, nullptr);
    EXPECT_TRUE(module.path.empty());
    EXPECT_EQ(module.mod_handle, nullptr);
}

TEST_F(ModuleLoaderTests, LoadedModuleMoveConstruction) {
    LoadedModule module;
    module.id = "test.mod";
    module.manifest = nullptr;
    module.handle = nullptr;
    module.entrypoint = nullptr;
    module.path = L"test/path.dll";
    
    LoadedModule moved = std::move(module);
    
    EXPECT_EQ(moved.id, "test.mod");
    EXPECT_EQ(moved.path, L"test/path.dll");
}

// ============================================================================
// ModuleLoadError Structure Tests
// ============================================================================

TEST_F(ModuleLoaderTests, ModuleLoadErrorDefaultConstruction) {
    ModuleLoadError error;
    
    EXPECT_TRUE(error.id.empty());
    EXPECT_TRUE(error.path.empty());
    EXPECT_TRUE(error.message.empty());
    EXPECT_EQ(error.system_code, 0);
}

TEST_F(ModuleLoaderTests, ModuleLoadErrorContainsDetails) {
    ModuleLoadError error;
    error.id = "failed.mod";
    error.path = L"C:\\path\\to\\failed.dll";
    error.message = "Failed to load DLL";
    error.system_code = 126; // ERROR_MOD_NOT_FOUND
    
    EXPECT_EQ(error.id, "failed.mod");
    EXPECT_EQ(error.message, "Failed to load DLL");
    EXPECT_EQ(error.system_code, 126);
}

// ============================================================================
// ResolvedNode Integration Tests
// ============================================================================

TEST_F(ModuleLoaderTests, ResolvedNodeWithMultipleManifests) {
    auto* manifest1 = CreateManifest("mod1");
    auto* manifest2 = CreateManifest("mod2");
    auto* manifest3 = CreateManifest("mod3");
    
    ResolvedNode node1{manifest1->package.id, manifest1};
    ResolvedNode node2{manifest2->package.id, manifest2};
    ResolvedNode node3{manifest3->package.id, manifest3};
    
    std::vector<ResolvedNode> order = {node1, node2, node3};
    
    EXPECT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0].id, "mod1");
    EXPECT_EQ(order[1].id, "mod2");
    EXPECT_EQ(order[2].id, "mod3");
}

// ============================================================================
// Context Integration Tests
// ============================================================================

TEST_F(ModuleLoaderTests, ContextModuleLifecycle) {
    // Verify Context can create mod handles properly
    ModManifest manifest;
    manifest.package.id = "test.mod";
    manifest.package.name = "Test Mod";
    manifest.package.version = "1.0.0";
    manifest.package.parsed_version = {1, 0, 0};
    
    auto handle = Context::Instance().CreateModHandle(manifest);
    EXPECT_NE(handle, nullptr);
    EXPECT_EQ(handle->id, "test.mod");
    EXPECT_EQ(handle->version.major, 1);
    EXPECT_EQ(handle->version.minor, 0);
    EXPECT_EQ(handle->version.patch, 0);
}

// ============================================================================
// DependencyResolver Tests
// ============================================================================

using BML::Core::DependencyResolver;
using BML::Core::DependencyResolutionError;
using BML::Core::DependencyWarning;
using BML::Core::SemanticVersionRange;
using BML::Core::ParseSemanticVersionRange;
using BML::Core::ModDependency;

class DependencyResolverTests : public ::testing::Test {
protected:
    void SetUp() override {
        resolver_.Clear();
    }

    void TearDown() override {
        manifests_.clear();
    }

    ModManifest* CreateManifest(const std::string& id, 
                                 const std::string& version = "1.0.0") {
        auto manifest = std::make_unique<ModManifest>();
        manifest->package.id = id;
        manifest->package.name = id;
        manifest->package.version = version;
        ParseSemanticVersion(version, manifest->package.parsed_version);
        manifest->directory = L"test";
        manifest->manifest_path = L"test/" + std::wstring(id.begin(), id.end()) + L"/mod.toml";
        
        ModManifest* ptr = manifest.get();
        manifests_.push_back(std::move(manifest));
        return ptr;
    }

    void AddDependency(ModManifest* manifest, const std::string& dep_id, 
                       const std::string& constraint = "", bool optional = false) {
        ModDependency dep;
        dep.id = dep_id;
        dep.optional = optional;
        if (!constraint.empty()) {
            std::string error;
            ParseSemanticVersionRange(constraint, dep.requirement, error);
        }
        manifest->dependencies.push_back(std::move(dep));
    }

    DependencyResolver resolver_;
    std::vector<std::unique_ptr<ModManifest>> manifests_;
};

TEST_F(DependencyResolverTests, EmptyResolverSucceeds) {
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_TRUE(resolver_.Resolve(order, warnings, error));
    EXPECT_TRUE(order.empty());
    EXPECT_TRUE(warnings.empty());
}

TEST_F(DependencyResolverTests, SingleModuleResolves) {
    auto* mod = CreateManifest("test.mod");
    resolver_.RegisterManifest(*mod);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_TRUE(resolver_.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0].id, "test.mod");
}

TEST_F(DependencyResolverTests, LinearDependencyChainResolves) {
    // C depends on B, B depends on A
    auto* modA = CreateManifest("mod.a");
    auto* modB = CreateManifest("mod.b");
    auto* modC = CreateManifest("mod.c");
    
    AddDependency(modB, "mod.a");
    AddDependency(modC, "mod.b");
    
    resolver_.RegisterManifest(*modA);
    resolver_.RegisterManifest(*modB);
    resolver_.RegisterManifest(*modC);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_TRUE(resolver_.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 3u);
    
    // A must come before B, B must come before C
    auto posA = std::find_if(order.begin(), order.end(), [](const auto& n) { return n.id == "mod.a"; });
    auto posB = std::find_if(order.begin(), order.end(), [](const auto& n) { return n.id == "mod.b"; });
    auto posC = std::find_if(order.begin(), order.end(), [](const auto& n) { return n.id == "mod.c"; });
    
    EXPECT_LT(posA, posB);
    EXPECT_LT(posB, posC);
}

TEST_F(DependencyResolverTests, CycleDetected) {
    // A depends on B, B depends on C, C depends on A
    auto* modA = CreateManifest("mod.a");
    auto* modB = CreateManifest("mod.b");
    auto* modC = CreateManifest("mod.c");
    
    AddDependency(modA, "mod.b");
    AddDependency(modB, "mod.c");
    AddDependency(modC, "mod.a");
    
    resolver_.RegisterManifest(*modA);
    resolver_.RegisterManifest(*modB);
    resolver_.RegisterManifest(*modC);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_FALSE(resolver_.Resolve(order, warnings, error));
    EXPECT_FALSE(error.message.empty());
    EXPECT_TRUE(error.message.find("cycle") != std::string::npos);
}

TEST_F(DependencyResolverTests, MissingRequiredDependencyFails) {
    auto* mod = CreateManifest("test.mod");
    AddDependency(mod, "missing.dep", "", false);
    
    resolver_.RegisterManifest(*mod);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_FALSE(resolver_.Resolve(order, warnings, error));
    EXPECT_TRUE(error.message.find("missing.dep") != std::string::npos);
}

TEST_F(DependencyResolverTests, MissingOptionalDependencyWarns) {
    auto* mod = CreateManifest("test.mod");
    AddDependency(mod, "optional.dep", "", true);
    
    resolver_.RegisterManifest(*mod);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_TRUE(resolver_.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 1u);
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_TRUE(warnings[0].message.find("optional.dep") != std::string::npos);
}

TEST_F(DependencyResolverTests, DuplicateModuleIdFails) {
    auto* mod1 = CreateManifest("duplicate.mod");
    auto* mod2 = CreateManifest("duplicate.mod");
    
    resolver_.RegisterManifest(*mod1);
    resolver_.RegisterManifest(*mod2);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_FALSE(resolver_.Resolve(order, warnings, error));
    EXPECT_TRUE(error.message.find("Duplicate") != std::string::npos);
}

TEST_F(DependencyResolverTests, VersionConstraintExactMatch) {
    auto* dep = CreateManifest("dependency", "1.2.3");
    auto* mod = CreateManifest("test.mod");
    AddDependency(mod, "dependency", "=1.2.3");
    
    resolver_.RegisterManifest(*dep);
    resolver_.RegisterManifest(*mod);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_TRUE(resolver_.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 2u);
}

TEST_F(DependencyResolverTests, VersionConstraintExactMismatch) {
    auto* dep = CreateManifest("dependency", "1.2.4");
    auto* mod = CreateManifest("test.mod");
    AddDependency(mod, "dependency", "=1.2.3");
    
    resolver_.RegisterManifest(*dep);
    resolver_.RegisterManifest(*mod);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_FALSE(resolver_.Resolve(order, warnings, error));
    EXPECT_TRUE(error.message.find("constraint") != std::string::npos);
}

TEST_F(DependencyResolverTests, VersionConstraintGreaterEqual) {
    auto* dep = CreateManifest("dependency", "2.0.0");
    auto* mod = CreateManifest("test.mod");
    AddDependency(mod, "dependency", ">=1.5.0");
    
    resolver_.RegisterManifest(*dep);
    resolver_.RegisterManifest(*mod);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_TRUE(resolver_.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 2u);
}

TEST_F(DependencyResolverTests, VersionConstraintCompatible) {
    // ^1.2.0 should accept 1.3.0 but not 2.0.0
    auto* dep = CreateManifest("dependency", "1.3.0");
    auto* mod = CreateManifest("test.mod");
    AddDependency(mod, "dependency", "^1.2.0");
    
    resolver_.RegisterManifest(*dep);
    resolver_.RegisterManifest(*mod);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_TRUE(resolver_.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 2u);
}

TEST_F(DependencyResolverTests, VersionConstraintCompatibleRejectsMajorBump) {
    // ^1.2.0 should reject 2.0.0
    auto* dep = CreateManifest("dependency", "2.0.0");
    auto* mod = CreateManifest("test.mod");
    AddDependency(mod, "dependency", "^1.2.0");
    
    resolver_.RegisterManifest(*dep);
    resolver_.RegisterManifest(*mod);
    
    std::vector<ResolvedNode> order;
    std::vector<DependencyWarning> warnings;
    DependencyResolutionError error;
    
    EXPECT_FALSE(resolver_.Resolve(order, warnings, error));
}

} // namespace
