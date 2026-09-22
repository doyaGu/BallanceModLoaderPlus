#ifndef BML_IVP_SURFACE_H
#define BML_IVP_SURFACE_H

#include "BML/IVP/Calls.h"
#include "BML/IVP/Geometry.h"

class IVP_Ray_Solver;
class IVP_Real_Object;

enum IVP_SURMAN_TYPE : std::int32_t {
    IVP_SURMAN_POLYGON = 0,
    IVP_SURMAN_BALL = 1,
};

class IVP_Vector_of_Ledges_16 : public IVP_U_BigVector<IVP_Compact_Ledge> {
public:
    IVP_Vector_of_Ledges_16()
        : IVP_U_BigVector<IVP_Compact_Ledge>(
              reinterpret_cast<void **>(elem_buffer), 16) {}

private:
    IVP_Compact_Ledge *elem_buffer[16];
};

class IVP_Vector_of_Ledges_256 : public IVP_U_BigVector<IVP_Compact_Ledge> {
public:
    IVP_Vector_of_Ledges_256()
        : IVP_U_BigVector<IVP_Compact_Ledge>(
              reinterpret_cast<void **>(elem_buffer), 256) {}

private:
    IVP_Compact_Ledge *elem_buffer[256];
};

// Slot order is the retail 11-slot table, including the destructor before
// get_type. Keeping that order matters when a Mod implements this interface.
class IVP_SurfaceManager {
public:
    static void set_ledge_specific_client_data(IVP_Compact_Ledge *ledge,
                                                unsigned int value) {
        if (ledge)
            ledge->set_client_data(value);
    }
    static unsigned int get_ledge_specific_client_data(
        const IVP_Compact_Ledge *ledge) {
        return ledge ? static_cast<unsigned int>(ledge->get_client_data()) : 0u;
    }

    virtual const IVP_Compact_Ledge *get_single_convex() const = 0;
    virtual void get_mass_center(IVP_U_Float_Point *output) const = 0;
    virtual void get_radius_and_radius_dev_to_given_center(
        const IVP_U_Float_Point *center, IVP_FLOAT *radius,
        IVP_FLOAT *radiusDeviation) const = 0;
    virtual void get_rotation_inertia(IVP_U_Float_Point *output) const = 0;
    virtual void get_all_ledges_within_radius(
        const IVP_U_Point *observerPositionObject, IVP_DOUBLE radius,
        const IVP_Compact_Ledge *rootLedge, IVP_Real_Object *otherObject,
        const IVP_Compact_Ledge *otherReferenceLedge,
        IVP_U_BigVector<IVP_Compact_Ledge> *result) = 0;
    virtual void get_all_terminal_ledges(
        IVP_U_BigVector<IVP_Compact_Ledge> *result) = 0;
    virtual void insert_all_ledges_hitting_ray(IVP_Ray_Solver *raySolver,
                                               IVP_Real_Object *object) = 0;
    virtual void add_reference_to_ledge(const IVP_Compact_Ledge *) {}
    virtual void remove_reference_to_ledge(const IVP_Compact_Ledge *) {}
    virtual ~IVP_SurfaceManager() = 0;
    virtual IVP_SURMAN_TYPE get_type() = 0;
};

inline IVP_SurfaceManager::~IVP_SurfaceManager() {
    BML::IVP::ABI::InvokeThisOr<void>(
        BML::IVP::ABI::Address::SurfaceManagerDestruct, this, [] {});
}

class IVP_SurfaceManager_Polygon : public IVP_SurfaceManager {
    friend struct BML_IvpSurfaceManagerPolygonLayoutCheck;

protected:
    const IVP_Compact_Surface *compact_surface;

public:
    explicit IVP_SurfaceManager_Polygon(
        const IVP_Compact_Surface *compactSurface)
        : compact_surface(compactSurface) {}

    const IVP_Compact_Surface *get_compact_surface() const {
        return compact_surface;
    }

    const IVP_Compact_Ledge *get_single_convex() const override {
        return BML::IVP::ABI::InvokeThis<const IVP_Compact_Ledge *>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonGetSingleConvex,
            const_cast<IVP_SurfaceManager_Polygon *>(this));
    }
    void get_mass_center(IVP_U_Float_Point *output) const override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonGetMassCenter,
            const_cast<IVP_SurfaceManager_Polygon *>(this), output);
    }
    void get_radius_and_radius_dev_to_given_center(
        const IVP_U_Float_Point *center, IVP_FLOAT *radius,
        IVP_FLOAT *radiusDeviation) const override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonGetRadius,
            const_cast<IVP_SurfaceManager_Polygon *>(this), center,
            radius, radiusDeviation);
    }
    void get_rotation_inertia(IVP_U_Float_Point *output) const override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonGetInertia,
            const_cast<IVP_SurfaceManager_Polygon *>(this), output);
    }
    void get_all_ledges_within_radius(
        const IVP_U_Point *observerPositionObject, IVP_DOUBLE radius,
        const IVP_Compact_Ledge *rootLedge, IVP_Real_Object *otherObject,
        const IVP_Compact_Ledge *otherReferenceLedge,
        IVP_U_BigVector<IVP_Compact_Ledge> *result) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonGetLedgesInRadius,
            this, observerPositionObject, radius, rootLedge, otherObject,
            otherReferenceLedge, result);
    }
    void get_all_terminal_ledges(
        IVP_U_BigVector<IVP_Compact_Ledge> *result) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonGetTerminalLedges,
            this, result);
    }
    void insert_all_ledges_hitting_ray(
        IVP_Ray_Solver *raySolver, IVP_Real_Object *object) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonRaycast,
            this, raySolver, object);
    }
    void add_reference_to_ledge(const IVP_Compact_Ledge *ledge) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonReferenceNoop,
            this, ledge);
    }
    void remove_reference_to_ledge(const IVP_Compact_Ledge *ledge) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonReferenceNoop,
            this, ledge);
    }
    ~IVP_SurfaceManager_Polygon() override = default;
    IVP_SURMAN_TYPE get_type() override {
        return BML::IVP::ABI::InvokeThis<IVP_SURMAN_TYPE>(
            BML::IVP::ABI::Address::SurfaceManagerPolygonGetType, this);
    }
};

struct BML_IvpSurfaceManagerPolygonLayoutCheck {
    static constexpr std::size_t compact_surface =
        offsetof(IVP_SurfaceManager_Polygon, compact_surface);
};

class IVP_SurfaceManager_Ball : public IVP_SurfaceManager {
protected:
    IVP_Compact_Ledge *compact_ledge;

public:
    const IVP_Compact_Ledge *get_compact_ledge() const {
        return compact_ledge;
    }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Vector_of_Ledges_16) == 0x4C);
static_assert(sizeof(IVP_Vector_of_Ledges_256) == 0x40C);
static_assert(sizeof(IVP_SurfaceManager) == 0x04);
static_assert(sizeof(IVP_SurfaceManager_Polygon) == 0x08);
static_assert(BML_IvpSurfaceManagerPolygonLayoutCheck::compact_surface == 0x04);
static_assert(sizeof(IVP_SurfaceManager_Ball) == 0x08);
#endif

#endif // BML_IVP_SURFACE_H
