#ifndef BML_IVP_TEMPLATES_H
#define BML_IVP_TEMPLATES_H

#include "BML/IVP/Material.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

class IVP_Template_Polygon;

// Ballance's convex-polygon builders consume this older transport format.
// Unlike the later compact templates below, all five lifecycle/query bodies
// retained by the game are callable in physics_RT.dll.
class IVP_Template_Point : public IVP_U_Point {};

class IVP_Template_Line {
public:
    std::uint16_t p[2];

    void set(std::uint16_t first, std::uint16_t second) noexcept {
        p[0] = first;
        p[1] = second;
    }
};

class IVP_Template_Surface {
public:
    IVP_Template_Surface() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::TemplateSurfaceConstruct, this,
            [this] {
                normal.set_to_zero();
                normal.hesse_val = 0.0;
                templ_poly = nullptr;
                n_lines = 0;
                lines = nullptr;
                revert_line = nullptr;
            });
    }
    ~IVP_Template_Surface() { close_surface(); }

    void close_surface() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateSurfaceClose, this);
    }
    int get_surface_index() {
        return BML::IVP::ABI::InvokeThis<int>(
            BML::IVP::ABI::Address::TemplateSurfaceGetIndex, this);
    }
    void set_line(int surface_line_index, int line_index, char revert) {
        lines[surface_line_index] = static_cast<std::uint16_t>(line_index);
        revert_line[surface_line_index] = revert;
    }

    IVP_U_Point normal;
    IVP_Template_Polygon *templ_poly;
    int n_lines;
    std::uint16_t *lines;
    char *revert_line;
};

class IVP_Template_Polygon {
public:
    IVP_Template_Polygon() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::TemplatePolygonConstruct, this,
            [this] {
                n_points = 0;
                points = nullptr;
                n_lines = 0;
                lines = nullptr;
                n_surfaces = 0;
                surfaces = nullptr;
            });
    }
    ~IVP_Template_Polygon() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplatePolygonDestruct, this);
    }

    int n_points;
    IVP_Template_Point *points;
    int n_lines;
    IVP_Template_Line *lines;
    int n_surfaces;
    IVP_Template_Surface *surfaces;
};

class IVP_Template_Object {
    friend struct BML_IvpTemplateObjectLayoutCheck;

public:
    struct Retail_Construction_Tag {};

    IVP_Template_Object() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::TemplateObjectConstruct, this,
            [this] { name = nullptr; });
    }
    ~IVP_Template_Object() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateObjectDestruct, this);
    }

    void set_name(const char *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateObjectSetName, this, value);
    }
    const char *get_name() const { return name; }

protected:
    // A retained complete derived constructor owns the base-construction call.
    // This tag supplies storage only, avoiding a second trip through 0x15E90.
    explicit IVP_Template_Object(Retail_Construction_Tag) noexcept {}

private:
    char *name;
};

struct BML_IvpTemplateObjectLayoutCheck {
    static constexpr std::size_t name = offsetof(IVP_Template_Object, name);
};

// The retail and nearby public interfaces add no state to the base object
// template. Keeping the distinct type matters for the IVP_Cluster constructor
// signature even though the x86 representation remains four bytes.
class IVP_Template_Cluster : public IVP_Template_Object {};

// This is the retail 0x70 layout. Ballance retains
// enable_piling_optimization at +0x10 but omits the nearby revision's pinned
// field; the retained Real Object constructor passes +0x10 to IVP_Core.
class IVP_Template_Real_Object : public IVP_Template_Object {
public:
    IVP_Template_Real_Object()
        : IVP_Template_Object(Retail_Construction_Tag{}) {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::TemplateRealObjectConstruct, this,
            [this] {
                // RVA 0x15EF0 first constructs the four-byte Object template,
                // then clears the complete 0x70-byte transport object before
                // installing these defaults. Reproduce that write set only
                // when the exact retail body cannot be resolved.
                *reinterpret_cast<char **>(
                    static_cast<IVP_Template_Object *>(this)) = nullptr;
                std::memset(nocoll_group_ident, 0,
                            sizeof(nocoll_group_ident));
                physical_unmoveable = IVP_FALSE;
                enable_piling_optimization = IVP_FALSE;
                material = nullptr;
                mass = 1.0;
                rot_inertia_is_factor = IVP_TRUE;
                rot_inertia.k[0] = 1.0f;
                rot_inertia.k[1] = 1.0f;
                rot_inertia.k[2] = 1.0f;
                rot_inertia.hesse_val = 0.0f;
                auto_check_rot_inertia = 0.03f;
                constexpr IVP_DOUBLE damping =
                    static_cast<IVP_DOUBLE>(0.01f);
                speed_damp_factor = damping;
                rot_speed_damp_factor.k[0] = damping;
                rot_speed_damp_factor.k[1] = damping;
                rot_speed_damp_factor.k[2] = damping;
                rot_speed_damp_factor.hesse_val = 0.0;
                extra_radius = 0.0f;
                mass_center_override = nullptr;
                client_data = nullptr;
            });
    }
    ~IVP_Template_Real_Object() = default;

    void set_nocoll_group_ident(const char *value) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::TemplateRealObjectSetNoCollGroup,
            this, value);
    }
    const char *get_nocoll_group_ident() const {
        return nocoll_group_ident;
    }

private:
    char nocoll_group_ident[8];

public:
    IVP_BOOL physical_unmoveable;
    IVP_BOOL enable_piling_optimization;
    IVP_Material *material;
    IVP_DOUBLE mass;
    IVP_BOOL rot_inertia_is_factor;
    IVP_U_Float_Point rot_inertia;
    IVP_FLOAT auto_check_rot_inertia;
    IVP_DOUBLE speed_damp_factor;
    IVP_U_Point rot_speed_damp_factor;
    IVP_FLOAT extra_radius;
    IVP_U_Matrix *mass_center_override;
    void *client_data;
};

struct IVP_Template_Ball {
    IVP_FLOAT radius = 0.0f;
};

struct IVP_Template_Phantom {
    IVP_Template_Phantom() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::TemplatePhantomConstruct, this,
            [this] {
                manage_intruding_objects = IVP_FALSE;
                manage_intruding_cores = IVP_FALSE;
                dont_check_for_unmoveables = IVP_FALSE;
                exit_policy_extra_radius = 0.5f;
                exit_policy_extra_time = 0.5f;
            });
    }

    IVP_BOOL manage_intruding_objects;
    IVP_BOOL manage_intruding_cores;
    IVP_BOOL dont_check_for_unmoveables;
    IVP_FLOAT exit_policy_extra_radius;
    IVP_FLOAT exit_policy_extra_time;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Template_Object) == 0x04);
static_assert(BML_IvpTemplateObjectLayoutCheck::name == 0x00);
static_assert(sizeof(IVP_Template_Point) == 0x20);
static_assert(sizeof(IVP_Template_Line) == 0x04);
static_assert(sizeof(IVP_Template_Surface) == 0x30);
static_assert(offsetof(IVP_Template_Surface, templ_poly) == 0x20);
static_assert(offsetof(IVP_Template_Surface, n_lines) == 0x24);
static_assert(offsetof(IVP_Template_Surface, lines) == 0x28);
static_assert(offsetof(IVP_Template_Surface, revert_line) == 0x2C);
static_assert(sizeof(IVP_Template_Polygon) == 0x18);
static_assert(offsetof(IVP_Template_Polygon, points) == 0x04);
static_assert(offsetof(IVP_Template_Polygon, lines) == 0x0C);
static_assert(offsetof(IVP_Template_Polygon, surfaces) == 0x14);
static_assert(sizeof(IVP_Template_Cluster) == 0x04);
static_assert(sizeof(IVP_Template_Real_Object) == 0x70);
static_assert(offsetof(IVP_Template_Real_Object, physical_unmoveable) == 0x0C);
static_assert(offsetof(IVP_Template_Real_Object,
                       enable_piling_optimization) == 0x10);
static_assert(offsetof(IVP_Template_Real_Object, material) == 0x14);
static_assert(offsetof(IVP_Template_Real_Object, mass) == 0x18);
static_assert(offsetof(IVP_Template_Real_Object, rot_inertia) == 0x24);
static_assert(offsetof(IVP_Template_Real_Object, speed_damp_factor) == 0x38);
static_assert(offsetof(IVP_Template_Real_Object, extra_radius) == 0x60);
static_assert(offsetof(IVP_Template_Real_Object, client_data) == 0x68);
static_assert(sizeof(IVP_Template_Ball) == 0x04);
static_assert(sizeof(IVP_Template_Phantom) == 0x14);
static_assert(offsetof(IVP_Template_Phantom, manage_intruding_objects) == 0x00);
static_assert(offsetof(IVP_Template_Phantom, dont_check_for_unmoveables) ==
              0x08);
static_assert(offsetof(IVP_Template_Phantom, exit_policy_extra_radius) == 0x0C);
static_assert(offsetof(IVP_Template_Phantom, exit_policy_extra_time) == 0x10);
#endif

#endif // BML_IVP_TEMPLATES_H
