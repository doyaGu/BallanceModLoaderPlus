#include "Behavior/PrototypeCatalog.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace {

using namespace BML::Behavior;

PrototypeInfo Prototype(CKGUID guid, std::string provider,
                        std::string name) {
    PrototypeInfo result;
    result.Ref.Guid = guid;
    result.Provider.Key = provider;
    result.Provider.Name = provider;
    result.Provider.Guid = CKGUID(0x7000, provider == "alpha" ? 1 : 2);
    result.Name = std::move(name);
    result.Category = "Tests/Behavior";
    result.Author = "BML";
    result.Description = "catalog fixture";
    result.Version = 1;
    result.CompatibleClass = CKCID_BEOBJECT;
    result.Managers.push_back({CKGUID(0x9000, 1), true});
    return result;
}

class FakePrototypeSource final : public PrototypeSource {
public:
    Status ReadDeclarations(std::vector<PrototypeInfo> &out) override {
        ++DeclarationReads;
        out = Declarations;
        return DeclarationStatus;
    }

    Status PrototypeCount(std::size_t &out) const override {
        out = Declarations.size();
        return DeclarationStatus;
    }

    Status ReadDeclaredLayout(CKGUID prototype, Layout &out) override {
        ++LayoutReads;
        out = Declared;
        out.Prototype = prototype;
        if (DuringLayout)
            DuringLayout();
        return LayoutStatus;
    }

    void TakeRetirements(std::vector<CKGUID> &out,
                         bool &retireAll) override {
        out = std::move(Retirements);
        Retirements.clear();
        retireAll = RetireAll;
        RetireAll = false;
    }

    bool TracksRetirement() const noexcept override { return Tracks; }

    std::vector<PrototypeInfo> Declarations;
    Layout Declared;
    Status DeclarationStatus;
    Status LayoutStatus;
    std::vector<CKGUID> Retirements;
    std::function<void()> DuringLayout;
    int DeclarationReads = 0;
    int LayoutReads = 0;
    bool RetireAll = false;
    bool Tracks = true;
};

struct CatalogFixture : testing::Test {
    CatalogFixture() {
        auto source = std::make_unique<FakePrototypeSource>();
        Source = source.get();
        Source->Declarations = {
            Prototype(CKGUID(0x2000, 2), "beta", "Second"),
            Prototype(CKGUID(0x1000, 1), "alpha", "First"),
        };
        Source->Declared.PrototypeName = "First";
        Source->Declared.Kind = BehaviorKind::Function;
        Source->Declared.CompatibleClass = CKCID_3DENTITY;
        Source->Declared.Slots.push_back(
            {SlotKind::Input, 0, 0, "In", CKGUID(), 0});
        Catalog = std::make_unique<PrototypeCatalog>(std::move(source));
    }

    FakePrototypeSource *Source = nullptr;
    std::unique_ptr<PrototypeCatalog> Catalog;
};

TEST_F(CatalogFixture, FindsCopiedDeclarationsInGuidOrder) {
    PrototypeQuery query;
    std::vector<PrototypeInfo> found;
    ASSERT_TRUE(Catalog->Find(query, found));
    ASSERT_EQ(found.size(), 2u);
    EXPECT_EQ(found[0].Name, "First");
    EXPECT_EQ(found[1].Name, "Second");
    EXPECT_NE(found[0].Ref.Generation, 0u);

    const std::uint64_t generation = found[0].Ref.Generation;
    Source->Declarations[1].Name = "mutated after copy";
    EXPECT_EQ(found[0].Name, "First");

    Source->Declarations[1].Name = "First";
    ASSERT_TRUE(Catalog->Find(query, found));
    EXPECT_EQ(found[0].Ref.Generation, generation);
    EXPECT_EQ(Source->DeclarationReads, 1);
}

TEST_F(CatalogFixture, AppliesOnlyExplicitExactFilters) {
    PrototypeQuery query;
    query.MatchName = true;
    query.Name = "First";
    query.MatchProvider = true;
    query.Provider = "ALPHA";
    query.RequiredManagers.push_back(CKGUID(0x9000, 1));
    std::vector<PrototypeInfo> found;
    ASSERT_TRUE(Catalog->Find(query, found));
    ASSERT_EQ(found.size(), 1u);
    EXPECT_EQ(found.front().Ref.Guid, CKGUID(0x1000, 1));

    query.Name = "first";
    ASSERT_TRUE(Catalog->Find(query, found));
    EXPECT_TRUE(found.empty());
}

TEST_F(CatalogFixture, DeclarationCountChangeRefreshesTheCatalog) {
    std::vector<PrototypeInfo> found;
    ASSERT_TRUE(Catalog->Find({}, found));
    ASSERT_EQ(found.size(), 2u);
    const std::uint64_t alphaGeneration = found.front().Ref.Generation;

    Source->Declarations.push_back(
        Prototype(CKGUID(0x3000, 3), "gamma", "Third"));
    ASSERT_TRUE(Catalog->Find({}, found));
    ASSERT_EQ(found.size(), 3u);
    EXPECT_EQ(found.front().Ref.Generation, alphaGeneration);
    EXPECT_EQ(Source->DeclarationReads, 2);
}

TEST_F(CatalogFixture, RetirementInvalidatesOldProviderGeneration) {
    std::vector<PrototypeInfo> found;
    ASSERT_TRUE(Catalog->Find({}, found));
    const PrototypeRef before = found.front().Ref;

    Source->Retirements.push_back(before.Guid);
    ASSERT_TRUE(Catalog->Find({}, found));
    ASSERT_EQ(found.size(), 2u);
    const auto replacement = std::find_if(
        found.begin(), found.end(), [&](const PrototypeInfo &candidate) {
            return candidate.Ref.Guid == before.Guid;
        });
    ASSERT_NE(replacement, found.end());
    EXPECT_NE(replacement->Ref.Generation, before.Generation);
    EXPECT_EQ(Catalog->Validate(before).Code, Error::PrototypeChanged);
}

TEST_F(CatalogFixture, RetirementSurvivesATemporaryDeclarationReadFailure) {
    std::vector<PrototypeInfo> found;
    ASSERT_TRUE(Catalog->Find({}, found));
    const PrototypeRef before = found.front().Ref;

    Source->Retirements.push_back(before.Guid);
    Source->DeclarationStatus = {
        Error::ContextExpired, CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
        "temporary read failure"};
    EXPECT_FALSE(Catalog->Find({}, found));

    Source->DeclarationStatus = {};
    ASSERT_TRUE(Catalog->Find({}, found));
    const auto replacement = std::find_if(
        found.begin(), found.end(), [&](const PrototypeInfo &candidate) {
            return candidate.Ref.Guid == before.Guid;
        });
    ASSERT_NE(replacement, found.end());
    EXPECT_NE(replacement->Ref.Generation, before.Generation);
}

TEST_F(CatalogFixture, DeclaredLayoutIsCachedPerProviderGeneration) {
    std::vector<PrototypeInfo> found;
    ASSERT_TRUE(Catalog->Find({}, found));
    const PrototypeRef first = found.front().Ref;

    Layout initial;
    ASSERT_TRUE(Catalog->DeclaredLayout(first, initial));
    EXPECT_TRUE(initial.MaterializedNow);
    EXPECT_EQ(initial.ProviderGeneration, first.Generation);
    EXPECT_EQ(initial.ProviderName, "alpha");
    EXPECT_EQ(initial.CompatibleClass, CKCID_3DENTITY);
    EXPECT_EQ(Source->LayoutReads, 1);

    Layout cached;
    ASSERT_TRUE(Catalog->DeclaredLayout(first, cached));
    EXPECT_FALSE(cached.MaterializedNow);
    EXPECT_EQ(Source->LayoutReads, 1);
    ASSERT_EQ(cached.Slots.size(), 1u);
    EXPECT_EQ(cached.Slots.front().Name, "In");
}

TEST_F(CatalogFixture, RejectsProviderChangeDuringMaterialization) {
    std::vector<PrototypeInfo> found;
    ASSERT_TRUE(Catalog->Find({}, found));
    const PrototypeRef first = found.front().Ref;
    Source->DuringLayout = [&] { Source->Retirements.push_back(first.Guid); };

    Layout layout;
    const Status status = Catalog->DeclaredLayout(first, layout);
    EXPECT_EQ(status.Code, Error::PrototypeChanged);
}

TEST_F(CatalogFixture, PropagatesSourceFailureWithoutPartialResults) {
    Source->DeclarationStatus = {
        Error::ContextExpired, CKERR_INVALIDOBJECT, CKBR_PARAMETERERROR,
        "source unavailable"};
    std::vector<PrototypeInfo> found = {Prototype(CKGUID(1, 1), "old", "old")};
    const Status status = Catalog->Find({}, found);
    EXPECT_EQ(status.Code, Error::ContextExpired);
    EXPECT_TRUE(found.empty());
}

} // namespace
