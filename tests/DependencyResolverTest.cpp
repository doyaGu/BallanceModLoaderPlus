#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "Core/DependencyResolver.h"
#include "Core/ModManifest.h"
#include "Core/SemanticVersion.h"

namespace {

BML::Core::ModInterfaceExport MakeInterfaceExport(const std::string &id,
                                                  const std::string &version,
                                                  const std::string &description = {}) {
    BML::Core::ModInterfaceExport iface;
    iface.interface_id = id;
    iface.version = version;
    iface.description = description;
    if (!BML::Core::ParseSemanticVersion(version, iface.parsed_version)) {
        ADD_FAILURE() << "Failed to parse interface version '" << version << "'";
    }
    return iface;
}

BML::Core::ModInterfaceRequirement MakeInterfaceRequirement(const std::string &id,
                                                             const std::string &expr,
                                                             bool optional = false) {
    BML::Core::ModInterfaceRequirement req;
    req.interface_id = id;
    req.optional = optional;
    req.requirement.raw_expression = expr;
    std::string error;
    if (!BML::Core::ParseSemanticVersionRange(expr, req.requirement, error)) {
        ADD_FAILURE() << "Failed to parse version range '" << expr << "': " << error;
    }
    return req;
}

BML::Core::ModManifest CreateManifest(const std::string &id,
                                      const std::string &version,
                                      std::vector<BML::Core::ModDependency> deps = {},
                                      std::vector<BML::Core::ModInterfaceExport> interfaces = {},
                                      std::vector<BML::Core::ModInterfaceRequirement> requires_ = {}) {
    BML::Core::ModManifest manifest;
    manifest.package.id = id;
    manifest.package.name = id;
    manifest.package.version = version;
    if (!BML::Core::ParseSemanticVersion(version, manifest.package.parsed_version)) {
        ADD_FAILURE() << "Failed to parse semantic version '" << version << "'";
    }
    manifest.dependencies = std::move(deps);
    manifest.interfaces = std::move(interfaces);
    manifest.requires_ = std::move(requires_);
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
    EXPECT_NE(error.message.find("requires missing module dependency"), std::string::npos);
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
    EXPECT_NE(warnings[0].message.find("Optional module dependency"), std::string::npos);
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
    EXPECT_NE(error.message.find("requires module 'core'"), std::string::npos);
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

TEST(DependencyResolverTest, InterfaceRequirementResolvesToProvider) {
    auto provider = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "provider", "1.0.0", {},
        std::vector{MakeInterfaceExport("x.y", "1.0.0")}
    ));
    auto consumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "consumer", "1.0.0", {}, {},
        std::vector{MakeInterfaceRequirement("x.y", ">=1.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*provider);
    resolver.RegisterManifest(*consumer);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    ASSERT_TRUE(resolver.Resolve(order, warnings, error)) << error.message;
    ASSERT_EQ(order.size(), 2u);

    auto index_of = [&](const std::string &id) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i].id == id) return i;
        }
        return order.size();
    };
    EXPECT_LT(index_of("provider"), index_of("consumer"));
}

TEST(DependencyResolverTest, InterfaceVersionMismatchFails) {
    auto provider = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "provider", "1.0.0", {},
        std::vector{MakeInterfaceExport("x.y", "1.0.0")}
    ));
    auto consumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "consumer", "1.0.0", {}, {},
        std::vector{MakeInterfaceRequirement("x.y", ">=2.0.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*provider);
    resolver.RegisterManifest(*consumer);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("requires interface"), std::string::npos);
    EXPECT_NE(error.message.find("x.y"), std::string::npos);
}

TEST(DependencyResolverTest, DuplicateInterfaceProvidersFails) {
    auto providerA = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "provider.a", "1.0.0", {},
        std::vector{MakeInterfaceExport("x.y", "1.0.0")}
    ));
    auto providerB = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "provider.b", "1.0.0", {},
        std::vector{MakeInterfaceExport("x.y", "2.0.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*providerA);
    resolver.RegisterManifest(*providerB);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("Interface 'x.y'"), std::string::npos);
    EXPECT_NE(error.message.find("declared by both"), std::string::npos);
}

TEST(DependencyResolverTest, OptionalInterfaceRequirementCanBeMissing) {
    auto consumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "consumer", "1.0.0", {}, {},
        std::vector{MakeInterfaceRequirement("x.y", ">=1.0", true)}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*consumer);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_TRUE(resolver.Resolve(order, warnings, error));
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0].id, "consumer");
    ASSERT_EQ(warnings.size(), 1u);
    EXPECT_NE(warnings[0].message.find("x.y"), std::string::npos);
}

TEST(DependencyResolverTest, MissingRequiredInterfaceFails) {
    auto consumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "consumer", "1.0.0", {}, {},
        std::vector{MakeInterfaceRequirement("x.y", ">=1.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*consumer);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    EXPECT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("requires interface"), std::string::npos);
    EXPECT_NE(error.message.find("x.y"), std::string::npos);
    EXPECT_NE(error.message.find("no module exports it"), std::string::npos);
}

TEST(DependencyResolverTest, InterfaceRequirementDoesNotShadowModuleDep) {
    // [dependencies] and [requires] are separate namespaces.
    // A module dep resolves against modules, not interfaces.
    auto moduleXY = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "x.y", "2.0.0"
    ));
    auto providerOther = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "other.provider", "1.0.0", {},
        std::vector{MakeInterfaceExport("other.provider.api", "1.0.0")}
    ));
    auto consumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "consumer", "1.0.0", std::vector{MakeDependency("x.y", ">=2.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*moduleXY);
    resolver.RegisterManifest(*providerOther);
    resolver.RegisterManifest(*consumer);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    // Module dep "x.y" resolves to the module; unrelated interfaces do not affect it.
    ASSERT_TRUE(resolver.Resolve(order, warnings, error)) << error.message;

    auto index_of = [&](const std::string &id) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i].id == id) return i;
        }
        return order.size();
    };
    EXPECT_LT(index_of("x.y"), index_of("consumer"));
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

TEST(DependencyResolverTest, SelfRequirementDoesNotCycle) {
    // Module exports and requires its own interface -- should resolve without cycle
    auto mod = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "selfmod", "1.0.0", {},
        std::vector{MakeInterfaceExport("x", "1.0.0")},
        std::vector{MakeInterfaceRequirement("x", ">=1.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*mod);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    ASSERT_TRUE(resolver.Resolve(order, warnings, error)) << error.message;
    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0].id, "selfmod");
}

TEST(DependencyResolverTest, DuplicateEdgeFromDepsAndRequiresIsDeduplicated) {
    // Both [dependencies] and [requires] target the same provider module.
    // Should resolve with only one edge (no double incoming count).
    auto provider = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "provider", "1.0.0", {},
        std::vector{MakeInterfaceExport("x.y", "1.0.0")}
    ));
    auto consumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "consumer", "1.0.0",
        std::vector{MakeDependency("provider", ">=1.0")},
        {},
        std::vector{MakeInterfaceRequirement("x.y", ">=1.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*provider);
    resolver.RegisterManifest(*consumer);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    ASSERT_TRUE(resolver.Resolve(order, warnings, error)) << error.message;
    ASSERT_EQ(order.size(), 2u);

    auto index_of = [&](const std::string &id) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (order[i].id == id) return i;
        }
        return order.size();
    };
    EXPECT_LT(index_of("provider"), index_of("consumer"));
}

TEST(DependencyResolverTest, InterfaceIdCollidingWithModuleIdFails) {
    auto moduleXY = std::make_unique<BML::Core::ModManifest>(CreateManifest("x.y", "1.0.0"));
    auto provider = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "provider", "1.0.0", {},
        std::vector{MakeInterfaceExport("x.y", "1.0.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*moduleXY);
    resolver.RegisterManifest(*provider);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    ASSERT_FALSE(resolver.Resolve(order, warnings, error));
    EXPECT_NE(error.message.find("Interface id 'x.y'"), std::string::npos);
    EXPECT_NE(error.message.find("module id"), std::string::npos);
    EXPECT_TRUE(warnings.empty());
}

TEST(DependencyResolverTest, MissingInterfaceAndModuleErrorsUseDistinctTerms) {
    auto moduleConsumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "module.consumer", "1.0.0", std::vector{MakeDependency("missing.module", ">=1.0")}
    ));
    auto interfaceConsumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "interface.consumer", "1.0.0", {}, {},
        std::vector{MakeInterfaceRequirement("missing.interface", ">=1.0")}
    ));

    {
        BML::Core::DependencyResolver resolver;
        resolver.RegisterManifest(*moduleConsumer);

        std::vector<BML::Core::ResolvedNode> order;
        std::vector<BML::Core::DependencyWarning> warnings;
        BML::Core::DependencyResolutionError error;
        ASSERT_FALSE(resolver.Resolve(order, warnings, error));
        EXPECT_NE(error.message.find("module dependency"), std::string::npos);
        EXPECT_NE(error.message.find("missing.module"), std::string::npos);
    }

    {
        BML::Core::DependencyResolver resolver;
        resolver.RegisterManifest(*interfaceConsumer);

        std::vector<BML::Core::ResolvedNode> order;
        std::vector<BML::Core::DependencyWarning> warnings;
        BML::Core::DependencyResolutionError error;
        ASSERT_FALSE(resolver.Resolve(order, warnings, error));
        EXPECT_NE(error.message.find("requires interface"), std::string::npos);
        EXPECT_NE(error.message.find("missing.interface"), std::string::npos);
    }
}

TEST(DependencyResolverTest, RequiresOutdatedVersionGeneratesWarning) {
    // When a [requires] interface version is at exact minimum, an outdated-version
    // warning should be generated (symmetric with [dependencies] outdated warnings).
    auto provider = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "provider", "1.0.0", {},
        std::vector{MakeInterfaceExport("x.y", "1.0.0")}
    ));
    auto consumer = std::make_unique<BML::Core::ModManifest>(CreateManifest(
        "consumer", "1.0.0", {}, {},
        std::vector{MakeInterfaceRequirement("x.y", ">=1.0.0")}
    ));

    BML::Core::DependencyResolver resolver;
    resolver.RegisterManifest(*provider);
    resolver.RegisterManifest(*consumer);

    std::vector<BML::Core::ResolvedNode> order;
    std::vector<BML::Core::DependencyWarning> warnings;
    BML::Core::DependencyResolutionError error;
    ASSERT_TRUE(resolver.Resolve(order, warnings, error)) << error.message;
    ASSERT_EQ(order.size(), 2u);

    // Should generate an outdated-version warning for the interface requirement
    ASSERT_GE(warnings.size(), 1u);
    bool foundOutdated = false;
    for (const auto &w : warnings) {
        if (w.mod_id == "consumer" && w.dependency_id == "x.y" &&
            w.message.find("minimum version") != std::string::npos) {
            foundOutdated = true;
            break;
        }
    }
    EXPECT_TRUE(foundOutdated) << "Expected outdated-version warning for [requires] interface";
}

} // namespace
