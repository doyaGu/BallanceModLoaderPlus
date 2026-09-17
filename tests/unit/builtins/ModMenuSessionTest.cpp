#include "ModMenu/ModMenuSession.h"

#include <limits>

#include <gtest/gtest.h>

#include <utility>

namespace {
    const ModMenuOwner Owner{"sample.mod", 7, 2};

    ModMenuSettingDocument BooleanSetting(std::string id, bool value) {
        return {{Owner, {}, std::move(id)}, {}, {}, IProperty::BOOLEAN, value};
    }

    ModMenuSettingDocument IntegerSetting(std::string id, int value) {
        return {{Owner, {}, std::move(id)}, {}, {}, IProperty::INTEGER, value};
    }

    ModMenuSettingDocument KeySetting(std::string id, int value) {
        return {{Owner, {}, std::move(id)}, {}, {}, IProperty::KEY, value};
    }

    ModMenuSettingDocument FloatSetting(std::string id, float value) {
        return {{Owner, {}, std::move(id)}, {}, {}, IProperty::FLOAT, value};
    }

    ModMenuDetailsActionDocument CategoryAction(
        std::string id, std::string label,
        std::vector<ModMenuSettingDocument> settings) {
        for (ModMenuSettingDocument &setting : settings)
            setting.key.category = id;
        return ModMenuCategoryDocument{{std::move(id)}, std::move(label), {},
                                       std::move(settings)};
    }

    ModMenuPageKey PageActionKey(std::string id, std::uint64_t generation) {
        return {Owner.id, std::move(id), generation};
    }

    ModMenuDetailsActionDocument PageAction(
        std::string id, std::string label, std::string description,
        std::uint64_t generation) {
        return ModMenuPageInfo{PageActionKey(std::move(id), generation),
                               std::move(label), std::move(description)};
    }

    ModMenuCategoryDocument &CategoryAt(
        ModMenuDocument &document, std::size_t index) {
        return std::get<ModMenuCategoryDocument>(
            document.detailsActions.at(index));
    }

    const ModMenuCategoryDocument &CategoryAt(
        const ModMenuDocument &document, std::size_t index) {
        return std::get<ModMenuCategoryDocument>(
            document.detailsActions.at(index));
    }

    ModMenuDocument MakeDocument(std::uint64_t schemaRevision = 3,
                                 std::uint64_t valueRevision = 11) {
        ModMenuDocument document;
        document.owner = Owner;
        document.name = "Sample Mod";
        document.author = "Sample Author";
        document.version = "1.0";
        document.description = "Sample description";
        document.status = "Native Mod";
        document.diagnostic = "Sample diagnostic";
        document.schemaRevision = schemaRevision;
        document.valueRevision = valueRevision;
        document.detailsActions = {
            CategoryAction(
                "general", "General",
                {BooleanSetting("enabled", true), IntegerSetting("count", 3)}),
            CategoryAction("input", "Input", {KeySetting("shortcut", 12)}),
        };
        return document;
    }

    ModMenuSettingKey Key(std::string category, std::string property) {
        return {Owner, std::move(category), std::move(property)};
    }
}

TEST(ModMenuSessionTest, SelectsStableIdsAndKeepsDraftsAcrossCategories) {
    ModMenuSession session;

    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::NoSelection);
    ASSERT_TRUE(session.SelectMod(Owner));
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::AwaitingDocument);
    ASSERT_TRUE(session.Reconcile(MakeDocument()));
    ASSERT_TRUE(session.SelectDetailsAction(ModMenuCategoryKey{"general"}));
    EXPECT_EQ(session.Edit(Key("general", "count"), 8), ModMenuEditResult::Changed);

    ASSERT_TRUE(session.SelectDetailsAction(ModMenuCategoryKey{"input"}));
    EXPECT_EQ(session.Edit(Key("input", "shortcut"), 20), ModMenuEditResult::Changed);
    EXPECT_TRUE(session.IsDirty());
    EXPECT_TRUE(session.CanApply());
    const ModMenuDetailsActionKey *selected = session.GetSelectedDetailsActionKey();
    ASSERT_NE(selected, nullptr);
    ASSERT_TRUE(std::holds_alternative<ModMenuCategoryKey>(*selected));
    EXPECT_EQ(std::get<ModMenuCategoryKey>(*selected).id, "input");
    ASSERT_NE(session.GetSelectedMod(), nullptr);
    EXPECT_EQ(*session.GetSelectedMod(), Owner);
}

TEST(ModMenuSessionTest, PreservesModInformationAcrossReconciliation) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));

    const ModMenuDocument *document = session.GetDocument();
    ASSERT_NE(document, nullptr);
    EXPECT_EQ(document->name, "Sample Mod");
    EXPECT_EQ(document->author, "Sample Author");
    EXPECT_EQ(document->version, "1.0");
    EXPECT_EQ(document->description, "Sample description");
    EXPECT_EQ(document->status, "Native Mod");
    EXPECT_EQ(document->diagnostic, "Sample diagnostic");
}

TEST(ModMenuSessionTest, BuildsEditsInDeclarationOrderRatherThanEditOrder) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));

    ASSERT_EQ(session.Edit(Key("input", "shortcut"), 20), ModMenuEditResult::Changed);
    ASSERT_EQ(session.Edit(Key("general", "enabled"), false), ModMenuEditResult::Changed);
    ASSERT_EQ(session.Edit(Key("general", "count"), 8), ModMenuEditResult::Changed);

    const std::optional<ModMenuSession::PreparedEdits> prepared = session.PrepareEdits();
    ASSERT_TRUE(prepared);
    const ModMenuEditBatch &batch = prepared->GetBatch();
    EXPECT_EQ(batch.owner, Owner);
    EXPECT_EQ(batch.expectedSchemaRevision, 3U);
    ASSERT_EQ(batch.edits.size(), 3U);
    EXPECT_EQ(batch.edits[0].key, Key("general", "enabled"));
    EXPECT_EQ(batch.edits[1].key, Key("general", "count"));
    EXPECT_EQ(batch.edits[2].key, Key("input", "shortcut"));
    EXPECT_EQ(batch.edits[2].type, IProperty::KEY);
    EXPECT_EQ(std::get<int>(batch.edits[2].baseline), 12);
    EXPECT_EQ(std::get<int>(batch.edits[2].value), 20);
}

TEST(ModMenuSessionTest, SameValueDoesNotCreateADraftAndRevertDropsOne) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));

    EXPECT_EQ(session.Edit(Key("general", "count"), 3), ModMenuEditResult::Unchanged);
    EXPECT_FALSE(session.IsDirty());
    EXPECT_EQ(session.Edit(Key("general", "count"), 8), ModMenuEditResult::Changed);
    EXPECT_EQ(session.Edit(Key("general", "count"), 3), ModMenuEditResult::Changed);
    EXPECT_FALSE(session.IsDirty());

    ASSERT_EQ(session.Edit(Key("general", "enabled"), false), ModMenuEditResult::Changed);
    session.Revert();
    EXPECT_FALSE(session.IsDirty());
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Ready);
    EXPECT_FALSE(session.PrepareEdits());
}

TEST(ModMenuSessionTest, RebasesUneditedValuesWhilePreservingAnEditedBaseline) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));
    ASSERT_EQ(session.Edit(Key("general", "enabled"), false), ModMenuEditResult::Changed);

    ModMenuDocument replacement = MakeDocument(3, 12);
    CategoryAt(replacement, 0).settings[1].value = 9;
    ASSERT_TRUE(session.Reconcile(std::move(replacement)));

    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Ready);
    EXPECT_EQ(session.GetSettingState(Key("general", "enabled")),
              ModMenuSettingState::Edited);
    ASSERT_NE(session.GetValue(Key("general", "enabled")), nullptr);
    EXPECT_EQ(std::get<bool>(*session.GetValue(Key("general", "enabled"))), false);
    const ModMenuDocument *document = session.GetDocument();
    ASSERT_NE(document, nullptr);
    EXPECT_EQ(std::get<int>(CategoryAt(*document, 0).settings[1].value), 9);

    const std::optional<ModMenuSession::PreparedEdits> prepared = session.PrepareEdits();
    ASSERT_TRUE(prepared);
    ASSERT_EQ(prepared->GetBatch().edits.size(), 1U);
    EXPECT_EQ(std::get<bool>(prepared->GetBatch().edits[0].baseline), true);
    EXPECT_EQ(std::get<bool>(prepared->GetBatch().edits[0].value), false);
}

TEST(ModMenuSessionTest, DetectsAnExternalChangeToAnEditedSetting) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));
    ASSERT_EQ(session.Edit(Key("general", "count"), 8), ModMenuEditResult::Changed);

    ModMenuDocument replacement = MakeDocument(3, 12);
    CategoryAt(replacement, 0).settings[1].value = 6;
    ASSERT_TRUE(session.Reconcile(std::move(replacement)));

    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Conflict);
    EXPECT_EQ(session.GetSettingState(Key("general", "count")),
              ModMenuSettingState::Conflict);
    ASSERT_NE(session.GetValue(Key("general", "count")), nullptr);
    EXPECT_EQ(std::get<int>(*session.GetValue(Key("general", "count"))), 8);
    EXPECT_TRUE(session.IsDirty());
    EXPECT_FALSE(session.CanApply());
    EXPECT_FALSE(session.PrepareEdits());

    session.Revert();
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Ready);
    ASSERT_NE(session.GetDocument(), nullptr);
    EXPECT_EQ(std::get<int>(CategoryAt(*session.GetDocument(), 0).settings[1].value), 6);
}

TEST(ModMenuSessionTest, SchemaAndTypeChangesMakeDraftsStale) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));
    ASSERT_EQ(session.Edit(Key("general", "count"), 8), ModMenuEditResult::Changed);

    EXPECT_TRUE(session.Reconcile(MakeDocument(4, 12)));
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Stale);
    EXPECT_EQ(session.GetSettingState(Key("general", "count")),
              ModMenuSettingState::Stale);
    EXPECT_EQ(session.Edit(Key("general", "enabled"), false), ModMenuEditResult::NotReady);

    session.Revert();
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Ready);
    ASSERT_EQ(session.Edit(Key("general", "count"), 8), ModMenuEditResult::Changed);

    ModMenuDocument replacement = MakeDocument(4, 13);
    CategoryAt(replacement, 0).settings[1].type = IProperty::KEY;
    ASSERT_TRUE(session.Reconcile(std::move(replacement)));
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Stale);
}

TEST(ModMenuSessionTest, AReplacementIdentityMarksTheOwnerGoneWithoutLosingDrafts) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));
    ASSERT_EQ(session.Edit(Key("general", "count"), 8), ModMenuEditResult::Changed);

    ModMenuDocument replacement = MakeDocument();
    replacement.owner.contentGeneration++;
    for (ModMenuDetailsActionDocument &action : replacement.detailsActions) {
        if (auto *category = std::get_if<ModMenuCategoryDocument>(&action)) {
            for (ModMenuSettingDocument &setting : category->settings)
                setting.key.owner = replacement.owner;
        }
    }
    ASSERT_TRUE(session.Reconcile(std::move(replacement)));

    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::OwnerGone);
    EXPECT_TRUE(session.IsDirty());
    EXPECT_EQ(session.GetDocument(), nullptr);
    EXPECT_FALSE(session.SelectMod({"another.mod", 1}));

    session.Revert();
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::OwnerGone);
    EXPECT_TRUE(session.SelectMod({"another.mod", 1}));
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::AwaitingDocument);
}

TEST(ModMenuSessionTest, AcceptsCommittedValuesAndUpdatesTheLocalRevision) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));
    ASSERT_EQ(session.Edit(Key("general", "count"), 8), ModMenuEditResult::Changed);

    std::optional<ModMenuSession::PreparedEdits> prepared = session.PrepareEdits();
    ASSERT_TRUE(prepared);
    EXPECT_TRUE(session.IsDirty());

    static_assert(noexcept(session.AcceptEdits(std::move(*prepared), 12)));
    session.AcceptEdits(std::move(*prepared), 12);
    EXPECT_FALSE(session.IsDirty());
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Ready);
    ASSERT_NE(session.GetDocument(), nullptr);
    EXPECT_EQ(session.GetDocument()->valueRevision, 12U);
    EXPECT_EQ(std::get<int>(CategoryAt(*session.GetDocument(), 0).settings[1].value), 8);
}

TEST(ModMenuSessionTest, RejectsWrongValueTypesAndInvalidDocuments) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(MakeDocument()));

    EXPECT_EQ(session.Edit(Key("input", "shortcut"), 20.0f),
              ModMenuEditResult::TypeMismatch);
    EXPECT_EQ(session.Edit(Key("missing", "value"), 1),
              ModMenuEditResult::UnknownSetting);

    ModMenuDocument invalid = MakeDocument();
    ModMenuSettingDocument duplicate = IntegerSetting("count", 4);
    duplicate.key.category = "general";
    CategoryAt(invalid, 0).settings.push_back(std::move(duplicate));
    EXPECT_FALSE(session.Reconcile(std::move(invalid)));
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::Ready);
    ASSERT_NE(session.GetDocument(), nullptr);
    EXPECT_EQ(session.GetDocument()->valueRevision, 11U);
}

TEST(ModMenuSessionTest, RequiresStableNonEmptyIdentity) {
    ModMenuSession session;
    EXPECT_FALSE(session.SelectMod({"", 1}));
    EXPECT_FALSE(session.SelectMod({"sample.mod", 0}));

    ASSERT_TRUE(session.SelectMod(Owner));
    ModMenuDocument invalid = MakeDocument();
    std::get<ModMenuCategoryDocument>(invalid.detailsActions[0]).key.id.clear();
    EXPECT_FALSE(session.Reconcile(std::move(invalid)));
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::AwaitingDocument);
}

TEST(ModMenuSessionTest, TreatsNanAsAStableFloatValue) {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    ModMenuDocument document = MakeDocument();
    ModMenuSettingDocument scale = FloatSetting("scale", nan);
    scale.key.category = "general";
    scale.label = "Scale";
    CategoryAt(document, 0).settings.push_back(std::move(scale));

    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(std::move(document)));
    EXPECT_EQ(session.Edit(Key("general", "scale"), nan), ModMenuEditResult::Unchanged);
    EXPECT_FALSE(session.IsDirty());
}

TEST(ModMenuSessionTest, SelectsUnifiedDetailsActionsByStableKey) {
    ModMenuDocument document = MakeDocument();
    document.pageRevision = 4;
    document.detailsActions.push_back(PageAction(
        "advanced", "Advanced", "Fine-grained controls", 1));
    document.detailsActions.push_back(PageAction(
        "about", "About", "Build information", 2));

    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(std::move(document)));
    ASSERT_TRUE(session.SelectDetailsAction(ModMenuCategoryKey{"general"}));
    const ModMenuDetailsActionKey *selected = session.GetSelectedDetailsActionKey();
    ASSERT_NE(selected, nullptr);
    ASSERT_TRUE(std::holds_alternative<ModMenuCategoryKey>(*selected));
    EXPECT_EQ(std::get<ModMenuCategoryKey>(*selected).id, "general");

    ASSERT_TRUE(session.SelectDetailsAction(PageActionKey("advanced", 1)));
    selected = session.GetSelectedDetailsActionKey();
    ASSERT_NE(selected, nullptr);
    ASSERT_TRUE(std::holds_alternative<ModMenuPageKey>(*selected));
    EXPECT_EQ(std::get<ModMenuPageKey>(*selected).id, "advanced");
    EXPECT_FALSE(session.SelectDetailsAction(PageActionKey("missing", 1)));
    selected = session.GetSelectedDetailsActionKey();
    ASSERT_NE(selected, nullptr);
    EXPECT_EQ(std::get<ModMenuPageKey>(*selected).id, "advanced");
}

TEST(ModMenuSessionTest, ReconciliationDropsASelectedPageThatWasUnregistered) {
    ModMenuDocument document = MakeDocument();
    document.pageRevision = 1;
    document.detailsActions.push_back(
        PageAction("advanced", "Advanced", "", 1));

    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(document));
    ASSERT_TRUE(session.SelectDetailsAction(PageActionKey("advanced", 1)));

    document.pageRevision = 2;
    document.detailsActions.pop_back();
    ASSERT_TRUE(session.Reconcile(std::move(document)));
    EXPECT_EQ(session.GetSelectedDetailsActionKey(), nullptr);
    ASSERT_NE(session.GetDocument(), nullptr);
    EXPECT_EQ(session.GetDocument()->pageRevision, 2U);
}

TEST(ModMenuSessionTest, ReRegistrationWithTheSameIdRequiresAFreshEnter) {
    ModMenuDocument document = MakeDocument();
    document.pageRevision = 1;
    document.detailsActions.push_back(
        PageAction("advanced", "Advanced", "", 7));

    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(document));
    ASSERT_TRUE(session.SelectDetailsAction(PageActionKey("advanced", 7)));

    document.pageRevision = 2;
    std::get<ModMenuPageInfo>(document.detailsActions.back()).key.generation = 8;
    ASSERT_TRUE(session.Reconcile(std::move(document)));
    EXPECT_EQ(session.GetSelectedDetailsActionKey(), nullptr);
}

TEST(ModMenuSessionTest, RejectsDuplicateOrUnlabelledPages) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));

    ModMenuDocument duplicate = MakeDocument();
    duplicate.detailsActions.push_back(
        PageAction("advanced", "Advanced", "", 1));
    duplicate.detailsActions.push_back(
        PageAction("advanced", "Another", "", 2));
    EXPECT_FALSE(session.Reconcile(std::move(duplicate)));

    ModMenuDocument unlabelled = MakeDocument();
    unlabelled.detailsActions.push_back(
        PageAction("advanced", "", "", 1));
    EXPECT_FALSE(session.Reconcile(std::move(unlabelled)));
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::AwaitingDocument);
}

TEST(ModMenuSessionTest, KeepsCategoryAndPageIdNamespacesIndependent) {
    ModMenuDocument document = MakeDocument();
    document.detailsActions.push_back(
        PageAction("general", "Advanced General", "", 1));

    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));
    ASSERT_TRUE(session.Reconcile(std::move(document)));

    ASSERT_TRUE(session.SelectDetailsAction(ModMenuCategoryKey{"general"}));
    const ModMenuDetailsActionKey *selected = session.GetSelectedDetailsActionKey();
    ASSERT_NE(selected, nullptr);
    EXPECT_TRUE(std::holds_alternative<ModMenuCategoryKey>(*selected));

    ASSERT_TRUE(session.SelectDetailsAction(PageActionKey("general", 1)));
    selected = session.GetSelectedDetailsActionKey();
    ASSERT_NE(selected, nullptr);
    EXPECT_TRUE(std::holds_alternative<ModMenuPageKey>(*selected));
}

TEST(ModMenuSessionTest, RejectsAPageKeyFromAnotherOwner) {
    ModMenuSession session;
    ASSERT_TRUE(session.SelectMod(Owner));

    ModMenuDocument wrongOwner = MakeDocument();
    wrongOwner.detailsActions.push_back(PageAction("advanced", "Advanced", "", 1));
    std::get<ModMenuPageInfo>(wrongOwner.detailsActions.back()).key.owner =
        "other.mod";
    EXPECT_FALSE(session.Reconcile(std::move(wrongOwner)));
    EXPECT_EQ(session.GetStatus(), ModMenuSessionStatus::AwaitingDocument);
}
