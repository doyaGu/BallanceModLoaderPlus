#ifndef BML_IVP_MATERIAL_H
#define BML_IVP_MATERIAL_H

#include "BML/IVP/Types.h"

#include <cstddef>

enum P_MATERIAL_TYPE : std::int32_t {
    P_MATERIAL_TYPE_UNINITIALIZED = -1,
    P_MATERIAL_TYPE_TERMINAL = 0,
    P_MATERIAL_TYPE_LAST = 1,
};

class IVP_Environment;
struct IVP_Contact_Situation;

class IVP_Material {
protected:
    struct Retail_Construction_Tag {};
    explicit IVP_Material(Retail_Construction_Tag) noexcept {}

public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    // Retail MaterialSimple construction writes only the anisotropic-friction
    // enable flag in this base. material_type remains caller-owned state.
    IVP_Material() : second_friction_x_enabled(IVP_FALSE) {}

    P_MATERIAL_TYPE material_type;
    IVP_BOOL second_friction_x_enabled;

    virtual IVP_DOUBLE get_friction_factor() = 0;
    virtual IVP_DOUBLE get_second_friction_factor() = 0;
    virtual IVP_DOUBLE get_elasticity() = 0;
    virtual IVP_DOUBLE get_adhesion() = 0;
    virtual const char *get_name() = 0;
    virtual ~IVP_Material() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MaterialDestruct, this, [] {});
    }

    void destroy() { delete this; }
};

class IVP_Material_Simple : public IVP_Material {
public:
    IVP_Material_Simple()
        : IVP_Material(Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MaterialSimpleConstruct,
            this,
            [this] {
                second_friction_x_enabled = IVP_FALSE;
                friction_value = 0.0;
                elasticity = 0.5;
                adhesion = 0.0;
            },
            0.0, 0.5);
    }
    IVP_Material_Simple(IVP_DOUBLE friction, IVP_DOUBLE elasticity)
        : IVP_Material(Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MaterialSimpleConstruct,
            this,
            [this, friction, elasticity] {
                second_friction_x_enabled = IVP_FALSE;
                friction_value = friction;
                this->elasticity = elasticity;
                adhesion = 0.0;
            },
            friction, elasticity);
    }

    IVP_DOUBLE get_friction_factor() override {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::MaterialSimpleGetFriction, this,
            [this] { return friction_value; });
    }
    IVP_DOUBLE get_second_friction_factor() override {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::MaterialSimpleGetSecondFriction, this,
            [this] { return second_friction_x; });
    }
    IVP_DOUBLE get_elasticity() override {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::MaterialSimpleGetElasticity, this,
            [this] { return elasticity; });
    }
    IVP_DOUBLE get_adhesion() override { return adhesion; }
    const char *get_name() override {
        return BML::IVP::ABI::InvokeThisOr<const char *>(
            BML::IVP::ABI::Address::MaterialSimpleGetName, this,
            [] { return "Simple material"; });
    }
    ~IVP_Material_Simple() override = default;

    IVP_DOUBLE friction_value;
    IVP_DOUBLE second_friction_x;
    IVP_DOUBLE elasticity;
    IVP_DOUBLE adhesion;
};

class IVP_Material_Manager {
    friend struct BML_IvpMaterialManagerLayoutCheck;

public:
    BML_IVP_RETAIL_ALLOCATED_OBJECT;

    // The nearby tree made this constructor protected, while physics_RT
    // itself uses it to build the default application environment.
    explicit IVP_Material_Manager(IVP_BOOL deleteOnEnvironmentDelete) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::MaterialManagerConstruct,
            this,
            [this, deleteOnEnvironmentDelete] {
                delete_on_env_delete = deleteOnEnvironmentDelete;
            },
            deleteOnEnvironmentDelete);
    }

    virtual IVP_Material *get_material_by_index(
        const IVP_U_Point *position, int index) {
        return BML::IVP::ABI::InvokeThis<IVP_Material *>(
            BML::IVP::ABI::Address::MaterialManagerGetByIndex,
            this, position, index);
    }
    virtual IVP_DOUBLE get_friction_factor(IVP_Contact_Situation *situation) {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::MaterialManagerGetFriction,
            this, situation);
    }
    virtual IVP_DOUBLE get_elasticity(IVP_Contact_Situation *situation) {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::MaterialManagerGetElasticity,
            this, situation);
    }
    virtual IVP_DOUBLE get_adhesion(IVP_Contact_Situation *situation) {
        return BML::IVP::ABI::InvokeThis<IVP_DOUBLE>(
            BML::IVP::ABI::Address::MaterialManagerGetAdhesion,
            this, situation);
    }
    virtual ~IVP_Material_Manager() = default;
    virtual void environment_will_be_deleted(IVP_Environment *environment) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::MaterialManagerEnvironmentDelete,
            this, environment);
    }

private:
    IVP_BOOL delete_on_env_delete;
};

struct BML_IvpMaterialManagerLayoutCheck {
    static constexpr std::size_t delete_flag =
        offsetof(IVP_Material_Manager, delete_on_env_delete);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Material) == 0x0C);
static_assert(sizeof(IVP_Material_Simple) == 0x30);
static_assert(sizeof(IVP_Material_Manager) == 0x08);
static_assert(BML_IvpMaterialManagerLayoutCheck::delete_flag == 0x04);
static_assert(offsetof(IVP_Material, material_type) == 0x04);
static_assert(offsetof(IVP_Material, second_friction_x_enabled) == 0x08);
static_assert(offsetof(IVP_Material_Simple, friction_value) == 0x10);
static_assert(offsetof(IVP_Material_Simple, adhesion) == 0x28);
#endif

#endif // BML_IVP_MATERIAL_H
