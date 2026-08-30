#include <gtest/gtest.h>

#include "Api/ObjectRefs.h"

#include "CKGlobals.h"
#include "CKObject.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>

namespace {

std::array<CKObject *, 64> g_CurrentObjects{};

class FakeObject {
public:
    FakeObject(CK_ID id, CKContext *context) {
        m_Object = ::new (m_Storage.data()) CKObject;
        m_Object->m_ID = id;
        m_Object->m_Name = nullptr;
        m_Object->m_ObjectFlags = 0;
        m_Object->m_Context = context;
    }

    CKObject *Get() const { return m_Object; }

private:
    alignas(CKObject) std::array<std::byte, sizeof(CKObject)> m_Storage{};
    CKObject *m_Object = nullptr;
};

} // namespace

CKObject *CKGetObject(CKContext *, CK_ID id) {
    return id < g_CurrentObjects.size() ? g_CurrentObjects[id] : nullptr;
}

void CKObject::Show(CK_OBJECT_SHOWOPTION) {}
CKBOOL CKObject::IsHiddenByParent() { return FALSE; }
int CKObject::CanBeHide() { return 0; }
CKObject::~CKObject() = default;
CK_CLASSID CKObject::GetClassID() { return CKCID_OBJECT; }
void CKObject::PreSave(CKFile *, CKDWORD) {}
CKStateChunk *CKObject::Save(CKFile *, CKDWORD) { return nullptr; }
CKERROR CKObject::Load(CKStateChunk *, CKFile *) { return CK_OK; }
void CKObject::PostLoad() {}
void CKObject::PreDelete() {}
void CKObject::CheckPreDeletion() {}
void CKObject::CheckPostDeletion() {}
int CKObject::GetMemoryOccupation() { return 0; }
CKBOOL CKObject::IsObjectUsed(CKObject *, CK_CLASSID) { return FALSE; }
CKERROR CKObject::PrepareDependencies(CKDependenciesContext &) { return CK_OK; }
CKERROR CKObject::RemapDependencies(CKDependenciesContext &) { return CK_OK; }
CKERROR CKObject::Copy(CKObject &, CKDependenciesContext &) { return CK_OK; }

namespace {

class ObjectRefsTest : public ::testing::Test {
protected:
    void SetUp() override {
        g_CurrentObjects.fill(nullptr);
        m_Context = reinterpret_cast<CKContext *>(this);
        m_Refs = std::make_unique<BML::ObjectRefs>(m_Context);
    }

    void TearDown() override {
        m_Refs.reset();
        g_CurrentObjects.fill(nullptr);
    }

    CKObject *Publish(FakeObject &object) {
        CKObject *published = object.Get();
        EXPECT_LT(published->GetID(), g_CurrentObjects.size());
        g_CurrentObjects[published->GetID()] = published;
        return published;
    }

    CKContext *m_Context = nullptr;
    std::unique_ptr<BML::ObjectRefs> m_Refs;
};

TEST_F(ObjectRefsTest, NullObjectsAndReferencesStayNull) {
    EXPECT_EQ(m_Refs->Issue(nullptr).Domain, 0u);
    EXPECT_EQ(m_Refs->Resolve({}), nullptr);
}

TEST_F(ObjectRefsTest, IssueRejectsObjectsOutsideTheCurrentWorld) {
    FakeObject unpublished(17, m_Context);
    EXPECT_EQ(m_Refs->Issue(unpublished.Get()).Domain, 0u);

    FakeObject foreign(23, reinterpret_cast<CKContext *>(static_cast<uintptr_t>(1)));
    Publish(foreign);
    EXPECT_EQ(m_Refs->Issue(foreign.Get()).Domain, 0u);

    FakeObject deleting(29, m_Context);
    CKObject *object = Publish(deleting);
    object->ModifyObjectFlags(CK_OBJECT_TOBEDELETED, 0);
    EXPECT_EQ(m_Refs->Issue(object).Domain, 0u);
}

TEST_F(ObjectRefsTest, RepeatedIssueKeepsExactReference) {
    FakeObject source(17, m_Context);
    CKObject *object = Publish(source);

    const BML_ObjectRef first = m_Refs->Issue(object);
    const BML_ObjectRef second = m_Refs->Issue(object);

    EXPECT_EQ(first.Domain, BML_OBJECT_DOMAIN_VIRTOOLS);
    EXPECT_EQ(first.Slot, 17u);
    EXPECT_NE(first.Generation, 0u);
    EXPECT_EQ(second.Domain, first.Domain);
    EXPECT_EQ(second.Slot, first.Slot);
    EXPECT_EQ(second.Generation, first.Generation);
    EXPECT_EQ(m_Refs->Resolve(first), object);
}

TEST_F(ObjectRefsTest, DeletionInvalidatesOnlyNamedSlots) {
    FakeObject firstSource(17, m_Context);
    FakeObject secondSource(23, m_Context);
    CKObject *firstObject = Publish(firstSource);
    CKObject *secondObject = Publish(secondSource);

    const BML_ObjectRef first = m_Refs->Issue(firstObject);
    const BML_ObjectRef second = m_Refs->Issue(secondObject);
    const CK_ID deleted = firstObject->GetID();
    m_Refs->Invalidate(&deleted, 1);

    EXPECT_EQ(m_Refs->Resolve(first), nullptr);
    EXPECT_EQ(m_Refs->Resolve(second), secondObject);
}

TEST_F(ObjectRefsTest, WorldResetInvalidatesEveryReference) {
    FakeObject firstSource(17, m_Context);
    FakeObject secondSource(23, m_Context);
    const BML_ObjectRef first = m_Refs->Issue(Publish(firstSource));
    const BML_ObjectRef second = m_Refs->Issue(Publish(secondSource));

    m_Refs->Reset();

    EXPECT_EQ(m_Refs->Resolve(first), nullptr);
    EXPECT_EQ(m_Refs->Resolve(second), nullptr);
}

TEST_F(ObjectRefsTest, ResolveRejectsWrongDomainGenerationAndDeletingObject) {
    FakeObject source(17, m_Context);
    CKObject *object = Publish(source);
    const BML_ObjectRef reference = m_Refs->Issue(object);

    BML_ObjectRef wrongDomain = reference;
    wrongDomain.Domain += 1;
    EXPECT_EQ(m_Refs->Resolve(wrongDomain), nullptr);

    BML_ObjectRef wrongSerial = reference;
    wrongSerial.Generation += 1;
    if (wrongSerial.Generation == 0)
        wrongSerial.Generation = 1;
    EXPECT_EQ(m_Refs->Resolve(wrongSerial), nullptr);

    object->ModifyObjectFlags(CK_OBJECT_TOBEDELETED, 0);
    EXPECT_EQ(m_Refs->Resolve(reference), nullptr);
}

TEST_F(ObjectRefsTest, CurrentObjectMustStillMatchTheIssuedReference) {
    FakeObject firstSource(17, m_Context);
    FakeObject replacementSource(17, m_Context);
    CKObject *firstObject = Publish(firstSource);
    const BML_ObjectRef first = m_Refs->Issue(firstObject);

    CKObject *replacement = Publish(replacementSource);
    EXPECT_EQ(m_Refs->Resolve(first), nullptr);

    const BML_ObjectRef second = m_Refs->Issue(replacement);
    EXPECT_NE(second.Generation, first.Generation);
    EXPECT_EQ(m_Refs->Resolve(second), replacement);
}

TEST_F(ObjectRefsTest, ResolveHotPathMeetsReleaseBudget) {
#ifndef NDEBUG
    GTEST_SKIP() << "Performance gate runs only in Release builds";
#else
    FakeObject source(17, m_Context);
    CKObject *object = Publish(source);
    const BML_ObjectRef reference = m_Refs->Issue(object);
    ASSERT_NE(reference.Domain, 0u);

    constexpr size_t kIterations = 1'000'000;
    uintptr_t observed = 0;
    const auto start = std::chrono::steady_clock::now();
    for (size_t index = 0; index < kIterations; ++index)
        observed += reinterpret_cast<uintptr_t>(m_Refs->Resolve(reference));
    const auto elapsed = std::chrono::steady_clock::now() - start;

    EXPECT_NE(observed, 0u);
    const double nanosecondsPerResolve =
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count()) /
        static_cast<double>(kIterations);
    EXPECT_LT(nanosecondsPerResolve, 150.0);
#endif
}

} // namespace
