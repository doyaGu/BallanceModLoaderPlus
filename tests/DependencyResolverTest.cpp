#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "Core/DependencyResolver.h"
#include "Core/ModManifest.h"
#include "Core/SemanticVersion.h"

namespace {

BML::Core::ModManifest CreateManifest(const std::string &id,
                                      const std::string &version,
                                      std::vector<BML::Core::ModDependency> deps = {}) {
    BML::Core::ModManifest manifest;
    manifest.package.id = id;
    manifest.package.name = id;
    manifest.package.version = version;
    if (!BML::Core::ParseSemanticVersion(version, manifest.package.parsed_version)) {
        ADD_FAILURE() << "Failed to parse semantic version '" << version << "'";
    }
    manifest.dependencies = std::move(deps);
    manifest.manifest_path = std::wstring(L"tests/") + std::wstring(id.begin(), id.end()) + L".toml";
    return manifest;
}

BML::Core::ModDependency MakeDependency(const std::string &id,
                                        const std::string &expr,
                                        bool optional = false) {
    BML::Core::ModDependency dep;
    dep.id = id;
    dep.optional = optional;
    dep.requirement.raw_expression = expr;
    std::string error;
    if (!BML::Core::ParseSemanticVersionRange(expr, dep.requirement, error)) {
        ADD_FAILURE() << "Failed to parse version range '" << expr << "': " << error;
    }
    return dep;
}

BML::Core::ModConflict MakeConflict(const std::string &id,
                                    const std::string &expr,
                                    const std::string &reason = {}) {
    BML::Core::ModConflict conflict;
    conflict.id = id;
    conflict.reason = reason;
    if (!expr.empty() && expr != "*") {
        conflict.requirement.raw_expression = expr;
        std::string error;
        if (!BML::Core::ParseSemanticVersionRange(expr, conflict.requirement, error)) {
            ADD_FAILURE() << "Failed to parse conflict range '" << expr << "': " << error;
        }
    } else {
        conflict.requirement.raw_expression = "*";
        conflict.requirement.parsed = false;
    }
    return conflict;
}

TEST(DependencyResolverTest, OrdersManifestsTopologically) {
    std::vector<std::unique_ptr<BML::Core::ModManifest>> manifests;
    manifests.push_back(std::make_unique<BML::Core::ModManifest>(CreateManifest("core", "1.0.0")));
    manifests.push_back(std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "addon", "1.0.0", std::vector{MakeDependency("core", ">=1.0")}
    )));
    manifests.push_back(std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "ui", "1.0.0", std::vector{MakeDependency("addon", "^1.0")}
    )));

    BML::Core::DependencyResolver resolver;
    for (const auto &manifest : manifests) {
        resolver.RegisterManifest(*manifest);
    }

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    ASSERT_TRUE(resolver.Resolve(order, warnings, error)) << error.message;
    ASSERT_EQ(order.size(), manifests.size());

    auto index_of = [&](const std::string &id) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i].id == id)
                return i;
        }
        return order.size();
    };

    EXPECT_LT(index_of("core"), index_of("addon"));
    EXPECT_LT(index_of("addon"), index_of("ui"));
}

TEST(DependencyResolverTest, MissingRequiredDependencyFails) {
    auto addon = CreateManifest(
        "addon", "1.0.0", std::vector{MakeDependency("missing", "^1.0")}
    );

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(addon);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("requires missing dependency"), std::string::npos);
    ASSERT_EQ(error.chain.size(), 2u);
    EXPECT_NE(error.chain[0].find("addon"), std::string::npos);
    EXPECT_EQ(error.chain[1], "missing");
}

TEST(DependencyResolverTest, OptionalDependencyCanBeMissing) {
    auto addon = CreateManifest(
        "addon", "1.0.0", std::vector{MakeDependency("optional.mod", "^1.0", true)}
    );

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(addon);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_TRUE(resolver.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0].id, "addon");
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("Optional dependency"), std::string::npos);
    EXPECT_NE(warnings[0].message.find("optional.mod"), std::string::npos);
    EXPECT_NE(warnings[0].message.find("addon"), std::string::npos);
}

TEST(DependencyResolverTest, VersionConstraintMismatchFails) {
    auto core = CreateManifest("core", "1.0.0");
    auto addon = CreateManifest(
        "addon", "1.0.0", std::vector{MakeDependency("core", ">=2.0")}
    );

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(core);
    resolver.RegisterManifest(addon);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("requires 'core'"), std::string::npos);
    ASSERT_EQ(error.chain.size(), 2u);
    EXPECT_NE(error.chain[0].find("addon"), std::string::npos);
    EXPECT_NE(error.chain[1].find("core"), std::string::npos);
}

TEST(DependencyResolverTest, DetectsCycles) {
    auto first = CreateManifest(
        "first", "1.0.0", std::vector{MakeDependency("second", "^1.0")}
    );
    auto second = CreateManifest(
        "second", "1.0.0", std::vector{MakeDependency("first", "^1.0")}
    );

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(first);
    resolver.RegisterManifest(second);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_EQ(error.message, "Detected dependency cycle");
    EXPECT_GE(error.chain.size(), 3u);
    // The cycle contains both "first" and "second", but the starting point
    // depends on iteration order of unordered_map, so we just verify both are present
    EXPECT_TRUE(std::find(error.chain.begin(), error.chain.end(), "first") != error.chain.end());
    EXPECT_TRUE(std::find(error.chain.begin(), error.chain.end(), "second") != error.chain.end());
}

TEST(DependencyResolverTest, GeneratesWarningsForOutdatedVersions) {
    auto core = CreateManifest("core", "1.0.0");
    auto addon = CreateManifest(
        "addon", "1.0.0", std::vector{MakeDependency("core", ">=1.0.0")}
    );

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(core);
    resolver.RegisterManifest(addon);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_TRUE(resolver.Resolve(order, warnings, error));
    
    // Should generate a warning because core is at exact minimum version
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_EQ(warnings[0].mod_id, "addon");
    EXPECT_EQ(warnings[0].dependency_id, "core");
    EXPECT_NE(warnings[0].message.find("minimum version"), std::string::npos);
}

TEST(DependencyResolverTest, NoWarningsForNewerVersions) {
    auto core = CreateManifest("core", "1.5.0");
    auto addon = CreateManifest(
        "addon", "1.0.0", std::vector{MakeDependency("core", ">=1.0.0")}
    );

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(core);
    resolver.RegisterManifest(addon);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_TRUE(resolver.Resolve(order, warnings, error));
    
    // Should NOT generate warnings because core is newer than minimum
    EXPECT_EQ(warnings.size(), 0u);
}

TEST(DependencyResolverTest, FailsOnDuplicateModuleIds) {
    auto first = CreateManifest("dup", "1.0.0");
    auto second = CreateManifest("dup", "2.0.0");

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(first);
    resolver.RegisterManifest(second);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("Duplicate module id"), std::string::npos);
    ASSERT_GE(error.chain.size(), 2u);
}

TEST(DependencyResolverTest, ConflictRulesBlockCoexistence) {
    auto runtime = CreateManifest("runtime", "1.0.0");
    auto addon = CreateManifest("addon", "1.0.0");
    addon.conflicts.push_back(MakeConflict("runtime", ">=1.0.0", "Requires legacy renderer"));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(runtime);
    resolver.RegisterManifest(addon);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("Conflict detected"), std::string::npos);
    ASSERT_EQ(error.chain.size(), 2u);
    EXPECT_NE(error.chain[0].find("addon"), std::string::npos);
    EXPECT_NE(error.chain[1].find("runtime"), std::string::npos);
}

TEST(DependencyResolverTest, SelfDependencyFails) {
    auto manifest = CreateManifest(
        "loop", "1.0.0", std::vector{MakeDependency("loop", ">=1.0")}
    );

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(manifest);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("cannot depend on itself"), std::string::npos);
    ASSERT_EQ(error.chain.size(), 1u);
    EXPECT_NE(error.chain[0].find("loop"), std::string::npos);
}

TEST(DependencyResolverTest, DuplicateOptionalWarningsAreDeduplicated) {
    auto manifest = CreateManifest("addon", "1.0.0",
        std::vector{
            MakeDependency("optional.mod", "^1.0", true),
            MakeDependency("optional.mod", "^1.0", true)
        });

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(manifest);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_TRUE(resolver.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 1u);
    ASSERT_EQ(warnings.size(), 1u) << "Optional dependency warning should be deduplicated";
    EXPECT_EQ(warnings[0].dependency_id, "optional.mod");
}

TEST(DependencyResolverTest, RegistrationOrderIsStableWithoutDependencies) {
    auto a = CreateManifest("a", "1.0.0");
    auto b = CreateManifest("b", "1.0.0");
    auto c = CreateManifest("c", "1.0.0");

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(b);
    resolver.RegisterManifest(c);
    resolver.RegisterManifest(a);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    ASSERT_TRUE(resolver.Resolve(order, warnings, error)) << error.message;
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0].id, "b");
    EXPECT_EQ(order[1].id, "c");
    EXPECT_EQ(order[2].id, "a");
}

} // namespace
