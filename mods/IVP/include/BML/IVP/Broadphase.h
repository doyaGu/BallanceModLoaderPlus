#ifndef BML_IVP_BROADPHASE_H
#define BML_IVP_BROADPHASE_H

#include "BML/IVP/Calls.h"
#include "BML/IVP/Types.h"

#include <cstddef>

class IVP_Collision;
class IVP_Environment;
class IVP_Hull_Manager;
class IVP_Real_Object;
class IVP_Ray_Solver;
class IVP_Ray_Solver_Group;
class IVP_Sphere_Solver;
class IVP_ov_tree_hash;

// Ballance retains all four class bodies. Keep the public field order from the
// retail x86 layout and route behavior to the mandatory physics_RT.dll.
class IVP_Range_Manager {
    // MSVC leaves an implicit dword after the vptr because the class contains
    // IVP_DOUBLE members and therefore has eight-byte class alignment. The
    // first real field is consequently at +0x08 in Ballance.
    IVP_BOOL bound_to_environment;

public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_Environment *environment;

    IVP_DOUBLE look_ahead_time_intra;
    IVP_DOUBLE look_ahead_max_radius_intra;
    IVP_DOUBLE look_ahead_min_distance_intra;
    IVP_DOUBLE look_ahead_max_distance_intra;
    IVP_DOUBLE look_ahead_min_seconds_intra;

    IVP_DOUBLE look_ahead_time_world;
    IVP_DOUBLE look_ahead_max_radius_world;
    IVP_DOUBLE look_ahead_min_distance_world;
    IVP_DOUBLE look_ahead_max_distance_world;
    IVP_DOUBLE look_ahead_min_seconds_world;

    IVP_Range_Manager(
        IVP_Environment *environmentIn, IVP_BOOL deleteOnEnvironmentDelete) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RangeManagerConstruct, this,
            environmentIn, deleteOnEnvironmentDelete);
    }

    virtual void get_coll_range_intra_objects(
        const IVP_Real_Object *first, const IVP_Real_Object *second,
        IVP_DOUBLE *firstRange, IVP_DOUBLE *secondRange) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RangeManagerGetIntraObjectRanges,
            this, first, second, firstRange, secondRange);
    }

    virtual IVP_DOUBLE get_coll_range_in_world(
        const IVP_Real_Object *object) {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::RangeManagerGetWorldRange,
            this, object);
    }

    virtual void environment_will_be_deleted(
        IVP_Environment *environmentIn) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RangeManagerEnvironmentDeleted,
            this, environmentIn);
    }
};

enum IVP_HULL_ELEM_TYPE : std::int32_t {
    IVP_HULL_ELEM_POLYGON = 0,
    IVP_HULL_ELEM_ANCHOR = 1,
    IVP_HULL_ELEM_OO_WATCHER = 2,
    IVP_HULL_ELEM_OO_CONNECTOR = 3,
};

class IVP_Listener_Hull {
    friend class IVP_Hull_Manager;

public:
    virtual IVP_HULL_ELEM_TYPE get_type() = 0;
    virtual void hull_limit_exceeded_event(
        IVP_Hull_Manager *, IVP_HTIME) = 0;
    virtual void hull_manager_is_going_to_be_deleted_event(
        IVP_Hull_Manager *) = 0;
    virtual void hull_manager_is_reset(IVP_FLOAT, IVP_FLOAT) {}

private:
    unsigned int minlist_index;
};

struct IVP_OV_Node_Data {
    int x;
    int y;
    int z;
    int rasterlevel;
    int sizelevel;
};

class IVP_OV_Node;

class IVP_OV_Element : public IVP_Listener_Hull {
protected:
    IVP_HULL_ELEM_TYPE get_type() override {
        return BML::IVP::ABI::InvokeThis<IVP_HULL_ELEM_TYPE>(
            BML::IVP::ABI::Address::OVElementGetType, this);
    }
    void hull_limit_exceeded_event(
        IVP_Hull_Manager *manager, IVP_HTIME intrusion) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVElementHullLimitExceeded,
            this, manager, intrusion);
    }
    void hull_manager_is_going_to_be_deleted_event(
        IVP_Hull_Manager *manager) override {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVElementHullManagerDeleted,
            this, manager);
    }

public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_OV_Element(IVP_Real_Object *object) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVElementConstruct, this, object);
    }
    virtual ~IVP_OV_Element() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVElementDestruct, this);
    }

    void add_oo_collision(IVP_Collision *collision) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVElementAddCollision,
            this, collision);
    }
    void remove_oo_collision(IVP_Collision *collision) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVElementRemoveCollision,
            this, collision);
    }
    void add_to_hull_manager(
        IVP_Hull_Manager *manager, IVP_DOUBLE hullTime) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVElementAddToHullManager,
            this, manager, hullTime);
    }

    IVP_OV_Node *node;
    IVP_Hull_Manager *hull_manager;
    IVP_U_Float_Point center;
    IVP_FLOAT radius;
    IVP_Real_Object *real_object;
    // The retained complete constructor/destructor own this allocation. A
    // union supplies the exact +0x28 storage without adding a second host-side
    // IVP_U_FVector lifetime around the DLL calls.
    union {
        IVP_U_FVector<IVP_Collision> collision_fvector;
    };
};

class IVP_OV_Node {
    friend class IVP_OV_Tree_Manager;

    ~IVP_OV_Node() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVNodeDestruct, this);
    }

public:
    IVP_OV_Node() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVNodeConstruct, this);
    }

    IVP_OV_Node_Data data;
    IVP_OV_Node *parent;
    // The exact complete constructor/destructor own both vector lifetimes.
    // Anonymous union members provide the retail storage without host-side
    // preconstruction or repeated teardown.
    union {
        IVP_U_Vector<IVP_OV_Node> children;
    };
    union {
        IVP_U_Vector<IVP_OV_Element> elements;
    };
};

class IVP_OV_Tree_Manager {
    friend class IVP_Ray_Solver;
    friend class IVP_Ray_Solver_Group;
    friend class IVP_Sphere_Solver;

public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    IVP_OV_Tree_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVTreeManagerConstruct, this);
    }
    ~IVP_OV_Tree_Manager() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVTreeManagerDestruct, this);
    }

    IVP_OV_Tree_Manager(const IVP_OV_Tree_Manager &) = delete;
    IVP_OV_Tree_Manager &operator=(const IVP_OV_Tree_Manager &) = delete;

    IVP_DOUBLE insert_ov_element(
        IVP_OV_Element *element, IVP_DOUBLE min_radius,
        IVP_DOUBLE max_radius,
        IVP_U_Vector<IVP_OV_Element> *colliding_balls) {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::OVTreeManagerInsertElement,
            this, element, min_radius, max_radius, colliding_balls);
    }
    void remove_ov_element(IVP_OV_Element *element) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::OVTreeManagerRemoveElement,
            this, element);
    }

private:
    IVP_DOUBLE power2(int exponent) const {
        return powerlist[exponent + 40];
    }

    void get_luf_coordinates_ws(
        const IVP_OV_Node *node, IVP_U_Float_Point *position,
        IVP_FLOAT *cubeSize) const {
        const IVP_DOUBLE raster = power2(node->data.rasterlevel);
        position->set(
            static_cast<IVP_FLOAT>(raster * node->data.x),
            static_cast<IVP_FLOAT>(raster * node->data.y),
            static_cast<IVP_FLOAT>(raster * node->data.z));
        *cubeSize = static_cast<IVP_FLOAT>(power2(node->data.sizelevel));
    }

    void get_center_coordinates_ws(
        const IVP_OV_Node *node, IVP_U_Float_Point *position,
        IVP_FLOAT *cubeSize) const {
        const IVP_DOUBLE raster = power2(node->data.rasterlevel);
        position->set(
            static_cast<IVP_FLOAT>(raster * (node->data.x + 1)),
            static_cast<IVP_FLOAT>(raster * (node->data.y + 1)),
            static_cast<IVP_FLOAT>(raster * (node->data.z + 1)));
        *cubeSize = static_cast<IVP_FLOAT>(power2(node->data.sizelevel));
    }

    IVP_DOUBLE powerlist[81];
    // The retained complete constructor/destructor own both vectors embedded
    // in this node. Keep only its storage active on the host side.
    union {
        IVP_OV_Node search_node;
    };
    IVP_U_Vector<IVP_OV_Element> *collision_partners;
    IVP_ov_tree_hash *hash_table;
    IVP_Environment *environment;
    IVP_OV_Node *root;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Listener_Hull) == 0x08);
static_assert(sizeof(IVP_OV_Node_Data) == 0x14);
static_assert(sizeof(IVP_OV_Element) == 0x30);
static_assert(offsetof(IVP_OV_Element, node) == 0x08);
static_assert(offsetof(IVP_OV_Element, center) == 0x10);
static_assert(offsetof(IVP_OV_Element, real_object) == 0x24);
static_assert(sizeof(IVP_OV_Node) == 0x28);
static_assert(offsetof(IVP_OV_Node, parent) == 0x14);
static_assert(offsetof(IVP_OV_Node, children) == 0x18);
static_assert(offsetof(IVP_OV_Node, elements) == 0x20);
static_assert(sizeof(IVP_OV_Tree_Manager) == 0x2C0);
static_assert(sizeof(IVP_Range_Manager) == 0x60);
static_assert(offsetof(IVP_Range_Manager, environment) == 0x0C);
static_assert(offsetof(IVP_Range_Manager, look_ahead_time_intra) == 0x10);
static_assert(offsetof(IVP_Range_Manager, look_ahead_min_seconds_intra) ==
              0x30);
static_assert(offsetof(IVP_Range_Manager, look_ahead_time_world) == 0x38);
static_assert(offsetof(IVP_Range_Manager, look_ahead_min_seconds_world) ==
              0x58);
#endif

#endif // BML_IVP_BROADPHASE_H
