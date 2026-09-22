#include "IvpTestAdapter.h"

#include "BML/IVP/Environment.h"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

struct MindistFixture {
    alignas(void *) std::array<std::byte, sizeof(IVP_Mindist)> storage{};

    IVP_Mindist *mindist() {
        return reinterpret_cast<IVP_Mindist *>(storage.data());
    }

    void initialize(IVP_Real_Object *first,
                    IVP_Real_Object *second,
                    IVP_FLOAT distance) {
        IVP_Mindist *value = mindist();
        IVP_Synapse_Real *firstSynapse = value->get_synapse(0);
        IVP_Synapse_Real *secondSynapse = value->get_synapse(1);
        firstSynapse->l_obj = first;
        secondSynapse->l_obj = second;
        firstSynapse->set_synapse_mindist(value);
        secondSynapse->set_synapse_mindist(value);
        *reinterpret_cast<IVP_FLOAT *>(storage.data() + 0x54u) = distance;
    }
};

struct RawMinList {
    std::uint16_t mallocedSize;
    std::uint16_t freeList;
    IVP_U_Min_List_Element *elements;
    IVP_FLOAT minValue;
    std::uint16_t firstLong;
    std::uint16_t firstElement;
    std::uint16_t counter;
    std::uint16_t reserved;
};

static_assert(sizeof(RawMinList) == sizeof(IVP_U_Min_List));

IVP_Mindist *g_validHullMindist = nullptr;
int g_recalculationCount = 0;

void __fastcall MindistDoImpact(IVP_Mindist *, void *) {}

IVP_HULL_ELEM_TYPE __fastcall PolygonHullType(
    IVP_Listener_Hull *, void *) {
    return IVP_HULL_ELEM_POLYGON;
}

IVP_MRC_TYPE __fastcall RecalculateMindist(IVP_Mindist *mindist, void *) {
    ++g_recalculationCount;
    return mindist == g_validHullMindist ? IVP_MRC_OK : IVP_MRC_BACKSIDE;
}

const BML::IVP::Test::RetailCallBinding kRetailCalls[] = {
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MindistRecalc, &RecalculateMindist),
    BML::IVP::Test::Bind(
        BML::IVP::ABI::Address::MindistDoImpact, &MindistDoImpact),
};

uintptr_t ResolveRetailCall(std::uint32_t rva) noexcept {
    return BML::IVP::Test::Resolve(rva, kRetailCalls);
}


class RecordingRadar final : public IVP_Radar {
public:
    void radar_hit(IVP_Radar_Hit *hit) override {
        ASSERT_LT(hitCount, static_cast<int>(hits.size()));
        hits[hitCount++] = *hit;
    }

    std::array<IVP_Radar_Hit, 4> hits{};
    int hitCount = 0;
};

void InstallPolygonVtable(IVP_Synapse_Real *synapse) {
    static void *vtable[] = {
        reinterpret_cast<void *>(&PolygonHullType),
        nullptr,
        nullptr,
        nullptr,
    };
    *reinterpret_cast<void ***>(synapse) = vtable;
}

} // namespace

extern "C" uintptr_t BML_IvpTestResolveRetailCall(
    std::uint32_t rva) noexcept {
    return ResolveRetailCall(rva);
}

namespace {

TEST(IvpMindistCompatibility,
     DetectsRetailRecursiveObjectsWithoutAddingAVirtualSlot) {
    MindistFixture fixture;
    std::array<void *, 8> ordinaryMindistVtable{};
    const std::uintptr_t imageBase =
        reinterpret_cast<std::uintptr_t>(&MindistDoImpact) -
        static_cast<std::uint32_t>(
            BML::IVP::ABI::Address::MindistDoImpact);
    void **recursiveMindistVtable = reinterpret_cast<void **>(
        imageBase + BML::IVP::ABI::MindistRecursiveVtableRva);

    *reinterpret_cast<void ***>(fixture.mindist()) =
        ordinaryMindistVtable.data();
    EXPECT_EQ(fixture.mindist()->is_recursive(), IVP_FALSE);

    *reinterpret_cast<void ***>(fixture.mindist()) = recursiveMindistVtable;
    EXPECT_EQ(fixture.mindist()->is_recursive(), IVP_TRUE);
}

TEST(IvpRadarNeighborhood,
     ReportsNearbyExactAndValidHullContactsInObjectOrientation) {
    std::array<std::byte, sizeof(IVP_Real_Object)> sourceStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> nearStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> farStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> hullStorage{};
    std::array<std::byte, sizeof(IVP_Real_Object)> invalidStorage{};
    auto *source = reinterpret_cast<IVP_Real_Object *>(sourceStorage.data());
    auto *nearObject = reinterpret_cast<IVP_Real_Object *>(nearStorage.data());
    auto *farObject = reinterpret_cast<IVP_Real_Object *>(farStorage.data());
    auto *hullObject = reinterpret_cast<IVP_Real_Object *>(hullStorage.data());
    auto *invalidObject = reinterpret_cast<IVP_Real_Object *>(
        invalidStorage.data());

    MindistFixture nearMindist;
    MindistFixture farMindist;
    MindistFixture validHullMindist;
    MindistFixture invalidHullMindist;
    nearMindist.initialize(source, nearObject, 1.25f);
    // Put the source in slot one to verify callback orientation.
    farMindist.initialize(farObject, source, 3.0f);
    validHullMindist.initialize(hullObject, source, 2.4f);
    invalidHullMindist.initialize(source, invalidObject, 0.5f);

    IVP_Synapse_Real *nearSynapse = nearMindist.mindist()->get_synapse(0);
    IVP_Synapse_Real *farSynapse = farMindist.mindist()->get_synapse(1);
    nearSynapse->next = farSynapse;
    farSynapse->next = nullptr;
    source->exact_synapses = nearSynapse;

    IVP_Synapse_Real *validHullSynapse =
        validHullMindist.mindist()->get_synapse(1);
    IVP_Synapse_Real *invalidHullSynapse =
        invalidHullMindist.mindist()->get_synapse(0);
    InstallPolygonVtable(validHullSynapse);
    InstallPolygonVtable(invalidHullSynapse);

    std::array<IVP_U_Min_List_Element, 2> hullEntries{};
    hullEntries[0].next = 1;
    hullEntries[0].value = 2.1f;
    hullEntries[0].element = validHullSynapse;
    hullEntries[1].next = IVP_U_MINLIST_UNUSED;
    hullEntries[1].value = 2.2f;
    hullEntries[1].element = invalidHullSynapse;

    auto *rawList = reinterpret_cast<RawMinList *>(
        &source->hull_manager.sorted_synapses);
    rawList->elements = hullEntries.data();
    rawList->firstElement = 0;
    rawList->firstLong = IVP_U_MINLIST_UNUSED;
    rawList->counter = 2;
    rawList->minValue = hullEntries[0].value;
    source->hull_manager.hull_value_last_vpsi = 0.5f;

    g_validHullMindist = validHullMindist.mindist();
    g_recalculationCount = 0;
    RecordingRadar radar;
    radar.max_range = 2.0;
    radar.max_relative_error = 0.01;

    source->do_radar_checking(&radar);

    ASSERT_EQ(radar.hitCount, 2);
    EXPECT_EQ(radar.hits[0].this_object, source);
    EXPECT_EQ(radar.hits[0].other_object, nearObject);
    EXPECT_DOUBLE_EQ(radar.hits[0].dist, 1.25);
    EXPECT_EQ(radar.hits[1].this_object, source);
    EXPECT_EQ(radar.hits[1].other_object, hullObject);
    EXPECT_NEAR(radar.hits[1].dist, 2.4, 1.0e-6);
    EXPECT_EQ(g_recalculationCount, 2);
}

} // namespace
