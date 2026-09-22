#ifndef BML_IVP_GEOMETRY_H
#define BML_IVP_GEOMETRY_H

#include "BML/IVP/Types.h"

#include <cstddef>
#include <cstdint>

class IVP_Compact_Ledge;
struct IVP_Compact_Ledgetree_Node;
class IVP_Compact_Triangle;

enum IVP_U_INTERSECT_TYPE : std::int32_t {
    IVP_U_INTERSECT_OK = 0,
    IVP_U_INTERSECT_NO_INTERSECTION = 1,
    IVP_U_INTERSECT_IDENTIC = 2,
    IVP_U_INTERSECT_PARALLEL = 3,
};

class IVP_U_Straight;

class IVP_U_Hesse : public IVP_U_Point {
public:
    void calc_hesse(const IVP_U_Point *point0, const IVP_U_Point *point1,
                    const IVP_U_Point *point2) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HesseCalculate,
            this, point0, point1, point2);
    }
    void calc_hesse(const IVP_U_Float_Point *point0,
                    const IVP_U_Float_Point *point1,
                    const IVP_U_Float_Point *point2) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HesseCalculateFromFloat,
            this, point0, point1, point2);
    }
    void calc_hesse_val(const IVP_U_Point *point) {
        hesse_val = -dot_product(point);
    }
    void proj_on_plane(const IVP_U_Point *point, IVP_U_Point *result) const {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HesseProjectOnPlane,
            const_cast<IVP_U_Hesse *>(this), point, result);
    }
    void mult_hesse(IVP_DOUBLE factor) {
        mult(factor);
        hesse_val *= factor;
    }
    void normize() {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::HesseNormalize, this);
    }
    IVP_DOUBLE get_dist(const IVP_U_Point *point) const {
        return dot_product(point) + hesse_val;
    }
    IVP_DOUBLE get_dist(const IVP_U_Float_Point *point) const {
        return dot_product(point) + hesse_val;
    }
    IVP_RETURN_TYPE calc_intersect_with(const IVP_U_Straight *straight,
                                        IVP_U_Point *pointOut) const;

    // The neighboring header swaps only four bytes of each double and marks
    // double-vector byte swapping unsupported. Expose the intended operation
    // without reproducing that source-version defect.
    void byte_swap() {
        BML::IVP::Detail::ByteSwapDouble(k[0]);
        BML::IVP::Detail::ByteSwapDouble(k[1]);
        BML::IVP::Detail::ByteSwapDouble(k[2]);
        BML::IVP::Detail::ByteSwapDouble(hesse_val);
    }
};

class IVP_U_Straight {
public:
    IVP_U_Point vec;
    IVP_U_Point start_point;

    IVP_U_Straight() = default;
    IVP_U_Straight(const IVP_U_Point *startPoint, const IVP_U_Point *direction) {
        set(startPoint, direction);
    }
    void set(const IVP_U_Point *startPoint, const IVP_U_Point *direction) {
        start_point.set(startPoint);
        vec.set(direction);
        vec.normize();
    }
    void set(const IVP_U_Float_Point *startPoint,
             const IVP_U_Float_Point *direction) {
        start_point.set(startPoint);
        vec.set(direction);
        vec.normize();
    }
    void calc_orthogonal_vec_from_point(const IVP_U_Point *point,
                                        IVP_U_Point *vectorOut) const {
        vectorOut->subtract(point, &start_point);
        const IVP_DOUBLE alongLine = vec.dot_product(vectorOut);
        vectorOut->add_multiple(&vec, -alongLine);
        vectorOut->mult(-1.0);
    }
    IVP_U_INTERSECT_TYPE calc_intersect_with(const IVP_U_Straight *other,
                                             IVP_U_Point *pointOut,
                                             IVP_DOUBLE *distanceOut) {
        IVP_U_Point normal;
        normal.calc_cross_product(&vec, &other->vec);
        constexpr IVP_DOUBLE epsilon = 1.0e-19;
        if (normal.quad_length() < epsilon * epsilon) {
            IVP_U_Point separation;
            other->calc_orthogonal_vec_from_point(&start_point, &separation);
            *distanceOut = separation.quad_length();
            return *distanceOut < epsilon * epsilon
                       ? IVP_U_INTERSECT_IDENTIC
                       : IVP_U_INTERSECT_PARALLEL;
        }

        IVP_U_Point planePoint0;
        IVP_U_Point planePoint1;
        IVP_U_Point planePoint2;
        planePoint0.set(&other->start_point);
        planePoint1.set(&other->start_point);
        planePoint1.add(&other->vec);
        planePoint2.set(&other->start_point);
        planePoint2.add(&normal);
        IVP_U_Hesse plane;
        plane.calc_hesse(&planePoint0, &planePoint1, &planePoint2);
        if (plane.calc_intersect_with(this, pointOut) != IVP_OK) {
            *distanceOut = 0.0;
            return IVP_U_INTERSECT_NO_INTERSECTION;
        }

        IVP_U_Point separation;
        other->calc_orthogonal_vec_from_point(pointOut, &separation);
        *distanceOut = separation.quad_length();
        return *distanceOut < epsilon * epsilon
                   ? IVP_U_INTERSECT_OK
                   : IVP_U_INTERSECT_NO_INTERSECTION;
    }
    IVP_DOUBLE get_quad_dist_to_point(IVP_U_Point *point) const {
        IVP_U_Point offset;
        offset.subtract(point, &start_point);
        IVP_U_Point cross;
        cross.calc_cross_product(&vec, &offset);
        return cross.quad_length();
    }
};

inline IVP_RETURN_TYPE IVP_U_Hesse::calc_intersect_with(
    const IVP_U_Straight *straight, IVP_U_Point *pointOut) const {
    return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
        BML::IVP::ABI::Address::HesseIntersectStraight,
        const_cast<IVP_U_Hesse *>(this), straight, pointOut);
}

class IVP_U_Plain : public IVP_U_Hesse {
public:
    IVP_U_Point start_point;
    IVP_U_Point vec1;
    IVP_U_Point vec2;

    explicit IVP_U_Plain(const IVP_U_Hesse *hesse) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::PlainConstructFromHesse, this, hesse);
    }
    IVP_U_Plain(const IVP_U_Point *point0, const IVP_U_Point *point1,
                const IVP_U_Point *point2) {
        calc_hesse(point0, point1, point2);
        start_point.set(point0);
        vec1.subtract(point1, point0);
        vec2.subtract(point2, point0);
    }
    IVP_RETURN_TYPE calc_intersect_with(const IVP_U_Hesse *other,
                                        IVP_U_Straight *straightOut) const {
        return BML::IVP::ABI::InvokeThis<IVP_RETURN_TYPE>(
            BML::IVP::ABI::Address::PlainIntersectPlane,
            const_cast<IVP_U_Plain *>(this), other, straightOut);
    }
};

inline IVP_RETURN_TYPE IVP_U_Point::set_crossing(
    IVP_U_Hesse *plane0, IVP_U_Hesse *plane1, IVP_U_Hesse *plane2) {
    IVP_U_Matrix equations;
    equations.rows[0].set(plane0);
    equations.rows[1].set(plane1);
    equations.rows[2].set(plane2);
    equations.vv.set_to_zero();
    const IVP_RETURN_TYPE result = equations.real_invert();
    IVP_U_Point rightSide(-plane0->hesse_val,
                          -plane1->hesse_val,
                          -plane2->hesse_val);
    equations.vmult3(&rightSide, this);
    return result;
}

class IVP_Compact_Poly_Point : public IVP_U_Float_Point {
public:
    IVP_Compact_Poly_Point() = default;
    explicit IVP_Compact_Poly_Point(const IVP_U_Point *point) { set(point); }

    void set_client_data(void *clientData) {
        *reinterpret_cast<void **>(&hesse_val) = clientData;
    }
    void *get_client_data() const {
        return *reinterpret_cast<void *const *>(&hesse_val);
    }
};

class IVP_Compact_Edge {
public:
    void byte_swap() {
        const std::uint32_t fields =
            ((packed & 0x0000FFFFu) << 16u) |
            ((packed & 0x7FFF0000u) >> 15u) |
            ((packed & 0x80000000u) >> 31u);
        packed = BML::IVP::Detail::ByteSwap32(fields);
    }

    int get_start_point_index() const {
        return static_cast<int>(packed & 0xFFFFu);
    }
    void set_start_point_index(int value) {
        packed = (packed & 0xFFFF0000u) |
                 (static_cast<std::uint32_t>(value) & 0xFFFFu);
    }
    int get_opposite_index() const {
        std::uint32_t value = (packed >> 16u) & 0x7FFFu;
        if ((value & 0x4000u) != 0)
            value |= 0xFFFF8000u;
        return static_cast<std::int32_t>(value);
    }
    void set_opposite_index(int value) {
        packed = (packed & 0x8000FFFFu) |
                 ((static_cast<std::uint32_t>(value) & 0x7FFFu) << 16u);
    }
    int get_is_virtual() const { return static_cast<int>(packed >> 31u); }
    void set_is_virtual(unsigned int value) {
        packed = (packed & 0x7FFFFFFFu) | ((value & 1u) << 31u);
    }

    const IVP_Compact_Triangle *get_triangle() const;
    IVP_Compact_Triangle *get_triangle();
    int get_edge_index() const {
        return static_cast<int>((reinterpret_cast<std::uintptr_t>(this) & 0xCu)
                                >> 2u) - 1;
    }
    const IVP_Compact_Edge *get_opposite() const {
        return this + get_opposite_index();
    }
    IVP_Compact_Edge *get_opposite() { return this + get_opposite_index(); }
    const IVP_Compact_Edge *get_next() const {
        static constexpr int offsets[3] = {1, 1, -2};
        return this + offsets[get_edge_index()];
    }
    IVP_Compact_Edge *get_next() {
        return const_cast<IVP_Compact_Edge *>(
            static_cast<const IVP_Compact_Edge *>(this)->get_next());
    }
    const IVP_Compact_Edge *get_prev() const {
        static constexpr int offsets[3] = {2, -1, -1};
        return this + offsets[get_edge_index()];
    }
    IVP_Compact_Edge *get_prev() {
        return const_cast<IVP_Compact_Edge *>(
            static_cast<const IVP_Compact_Edge *>(this)->get_prev());
    }
    const IVP_Compact_Ledge *get_compact_ledge() const;
    const IVP_Compact_Poly_Point *get_start_point(
        const IVP_Compact_Ledge *ledge) const;

private:
    std::uint32_t packed = 0;
};

class IVP_Compact_Triangle {
public:
    void byte_swap() {
        const std::uint32_t fields =
            ((packed & 0x00000FFFu) << 20u) |
            ((packed & 0x00FFF000u) >> 4u) |
            ((packed & 0x7F000000u) >> 23u) |
            ((packed & 0x80000000u) >> 31u);
        packed = BML::IVP::Detail::ByteSwap32(fields);
        edges[0].byte_swap();
        edges[1].byte_swap();
        edges[2].byte_swap();
    }

    int get_tri_index() const { return static_cast<int>(packed & 0xFFFu); }
    void set_tri_index(int value) {
        packed = (packed & ~0xFFFu) | (static_cast<std::uint32_t>(value) & 0xFFFu);
    }
    int get_pierce_index() const {
        return static_cast<int>((packed >> 12u) & 0xFFFu);
    }
    void set_pierce_index(int value) {
        packed = (packed & ~(0xFFFu << 12u)) |
                 ((static_cast<std::uint32_t>(value) & 0xFFFu) << 12u);
    }
    int get_material_index() const {
        return static_cast<int>((packed >> 24u) & 0x7Fu);
    }
    void set_material_index(int value) {
        packed = (packed & ~(0x7Fu << 24u)) |
                 ((static_cast<std::uint32_t>(value) & 0x7Fu) << 24u);
    }
    int get_is_virtual() const { return static_cast<int>(packed >> 31u); }
    void set_is_virtual(unsigned int value) {
        packed = (packed & 0x7FFFFFFFu) | ((value & 1u) << 31u);
    }
    const IVP_Compact_Edge *get_first_edge() const { return edges; }
    IVP_Compact_Edge *get_first_edge() { return edges; }
    const IVP_Compact_Edge *get_edge(int index) const { return &edges[index]; }
    IVP_Compact_Edge *get_edge(int index) { return &edges[index]; }
    const IVP_Compact_Triangle *get_next_tri() const { return this + 1; }
    IVP_Compact_Triangle *get_next_tri() { return this + 1; }
    const IVP_Compact_Ledge *get_compact_ledge() const;

private:
    std::uint32_t packed = 0;
    IVP_Compact_Edge edges[3];
};

class IVP_Compact_Ledge {
public:
    void byte_swap() {
        point_offset = static_cast<std::int32_t>(
            BML::IVP::Detail::ByteSwap32(
                static_cast<std::uint32_t>(point_offset)));
        node_offset_or_client_data = static_cast<std::int32_t>(
            BML::IVP::Detail::ByteSwap32(
                static_cast<std::uint32_t>(node_offset_or_client_data)));
        const std::uint32_t highByte = flags_and_size << 24u;
        const std::uint32_t reordered =
            ((highByte & 0x03000000u) << 6u) |
            ((highByte & 0x0C000000u) << 2u) |
            ((highByte & 0xF0000000u) >> 4u) |
            (flags_and_size >> 8u);
        flags_and_size = BML::IVP::Detail::ByteSwap32(reordered);
        n_triangles = static_cast<std::int16_t>(
            BML::IVP::Detail::ByteSwap16(
                static_cast<std::uint16_t>(n_triangles)));
        for_future_use = static_cast<std::int16_t>(
            BML::IVP::Detail::ByteSwap16(
                static_cast<std::uint16_t>(for_future_use)));
    }

    void byte_swap_all(
        IVP_U_BigVector<IVP_Compact_Poly_Point> *preSwappedPoints) {
        IVP_Compact_Poly_Point *points = get_point_array();
        IVP_Compact_Triangle *triangles = get_first_triangle();
        const int triangleCount = n_triangles;
        for (int triangleIndex = 0; triangleIndex < triangleCount;
             ++triangleIndex) {
            const int pointIndices[3] = {
                triangles[triangleIndex].get_edge(0)->get_start_point_index(),
                triangles[triangleIndex].get_edge(1)->get_start_point_index(),
                triangles[triangleIndex].get_edge(2)->get_start_point_index(),
            };
            for (int pointIndex : pointIndices) {
                IVP_Compact_Poly_Point *point = &points[pointIndex];
                bool alreadySwapped = false;
                if (preSwappedPoints) {
                    for (int index = 0; index < preSwappedPoints->len(); ++index) {
                        if (preSwappedPoints->element_at(index) == point) {
                            alreadySwapped = true;
                            break;
                        }
                    }
                    if (!alreadySwapped)
                        preSwappedPoints->add(point);
                }
                if (!alreadySwapped)
                    point->byte_swap();
            }
            triangles[triangleIndex].byte_swap();
        }
        byte_swap();
    }

    const IVP_Compact_Poly_Point *get_point_array() const {
        return reinterpret_cast<const IVP_Compact_Poly_Point *>(
            reinterpret_cast<const char *>(this) + point_offset);
    }
    IVP_Compact_Poly_Point *get_point_array() {
        return const_cast<IVP_Compact_Poly_Point *>(
            static_cast<const IVP_Compact_Ledge *>(this)->get_point_array());
    }
    const IVP_Compact_Triangle *get_first_triangle() const {
        return reinterpret_cast<const IVP_Compact_Triangle *>(this + 1);
    }
    IVP_Compact_Triangle *get_first_triangle() {
        return reinterpret_cast<IVP_Compact_Triangle *>(this + 1);
    }
    IVP_BOOL is_terminal() const {
        return (flags_and_size & 0x3u) == 0 ? IVP_TRUE : IVP_FALSE;
    }
    IVP_BOOL is_compact() const {
        return ((flags_and_size >> 2u) & 0x3u) != 0 ? IVP_TRUE : IVP_FALSE;
    }
    int get_n_triangles() const { return n_triangles; }
    int get_size() const { return static_cast<int>(flags_and_size >> 8u) * 16; }
    int get_n_points() const {
        return static_cast<int>(flags_and_size >> 8u) - n_triangles - 1;
    }
    int get_client_data() const { return node_offset_or_client_data; }
    void set_client_data(unsigned int value) {
        node_offset_or_client_data = static_cast<std::int32_t>(value);
    }
    const IVP_Compact_Ledgetree_Node *get_ledgetree_node() const {
        return reinterpret_cast<const IVP_Compact_Ledgetree_Node *>(
            reinterpret_cast<const char *>(this) + node_offset_or_client_data);
    }

private:
    std::int32_t point_offset = 0;
    std::int32_t node_offset_or_client_data = 0;
    std::uint32_t flags_and_size = 0;
    std::int16_t n_triangles = 0;
    std::int16_t for_future_use = 0;
};

inline const IVP_Compact_Triangle *IVP_Compact_Edge::get_triangle() const {
    return reinterpret_cast<const IVP_Compact_Triangle *>(
        reinterpret_cast<std::uintptr_t>(this) & ~std::uintptr_t{0xF});
}
inline IVP_Compact_Triangle *IVP_Compact_Edge::get_triangle() {
    return const_cast<IVP_Compact_Triangle *>(
        static_cast<const IVP_Compact_Edge *>(this)->get_triangle());
}
inline const IVP_Compact_Ledge *IVP_Compact_Edge::get_compact_ledge() const {
    const IVP_Compact_Triangle *triangle = get_triangle();
    triangle -= triangle->get_tri_index();
    return reinterpret_cast<const IVP_Compact_Ledge *>(triangle) - 1;
}
inline const IVP_Compact_Poly_Point *IVP_Compact_Edge::get_start_point(
    const IVP_Compact_Ledge *ledge) const {
    return &ledge->get_point_array()[get_start_point_index()];
}
inline const IVP_Compact_Ledge *IVP_Compact_Triangle::get_compact_ledge() const {
    return reinterpret_cast<const IVP_Compact_Ledge *>(this - get_tri_index()) - 1;
}

struct IVP_Compact_Ledgetree_Node {
    std::int32_t offset_right_node;
    std::int32_t offset_compact_ledge;
    IVP_U_Float_Point3 center;
    IVP_FLOAT radius;
    std::uint8_t box_sizes[3];
    std::uint8_t free_0;

    const IVP_Compact_Ledge *get_compact_ledge() const {
        return reinterpret_cast<const IVP_Compact_Ledge *>(
            reinterpret_cast<const char *>(this) + offset_compact_ledge);
    }
    const IVP_Compact_Ledgetree_Node *left_son() const { return this + 1; }
    const IVP_Compact_Ledgetree_Node *right_son() const {
        return reinterpret_cast<const IVP_Compact_Ledgetree_Node *>(
            reinterpret_cast<const char *>(this) + offset_right_node);
    }
    IVP_BOOL is_terminal() const {
        return offset_right_node == 0 ? IVP_TRUE : IVP_FALSE;
    }
    const IVP_Compact_Ledge *get_compact_hull() const {
        return offset_compact_ledge == 0 ? nullptr : get_compact_ledge();
    }
    void byte_swap() {
        offset_right_node = static_cast<std::int32_t>(
            BML::IVP::Detail::ByteSwap32(
                static_cast<std::uint32_t>(offset_right_node)));
        offset_compact_ledge = static_cast<std::int32_t>(
            BML::IVP::Detail::ByteSwap32(
                static_cast<std::uint32_t>(offset_compact_ledge)));
        center.byte_swap();
        BML::IVP::Detail::ByteSwapFloat(radius);
    }
    void byte_swap_all(
        IVP_U_BigVector<IVP_Compact_Poly_Point> *) {
        if (!is_terminal()) {
            const_cast<IVP_Compact_Ledgetree_Node *>(left_son())
                ->byte_swap_all(nullptr);
            const_cast<IVP_Compact_Ledgetree_Node *>(right_son())
                ->byte_swap_all(nullptr);
        }
        byte_swap();
    }
};

struct IVP_Compact_Surface {
    IVP_U_Float_Point3 mass_center;
    IVP_U_Float_Point3 rotation_inertia;
    IVP_FLOAT upper_limit_radius;
    union {
        struct {
            unsigned int max_factor_surface_deviation : 8;
            int byte_size : 24;
        };
        std::uint32_t factor_and_size;
    };
    std::int32_t offset_ledgetree_root;
    union {
        std::int32_t reserved[3];
        int dummy[3];
    };

    std::uint8_t get_max_factor_surface_deviation() const {
        return static_cast<std::uint8_t>(factor_and_size & 0xFFu);
    }
    int get_size() const { return static_cast<int>(factor_and_size >> 8u); }
    const IVP_Compact_Ledgetree_Node *get_compact_ledge_tree_root() const {
        return reinterpret_cast<const IVP_Compact_Ledgetree_Node *>(
            reinterpret_cast<const char *>(this) + offset_ledgetree_root);
    }

    void byte_swap() {
        mass_center.byte_swap();
        rotation_inertia.byte_swap();
        BML::IVP::Detail::ByteSwapFloat(upper_limit_radius);
        const std::uint32_t reordered =
            (factor_and_size << 24u) | (factor_and_size >> 8u);
        factor_and_size = BML::IVP::Detail::ByteSwap32(reordered);
        offset_ledgetree_root = static_cast<std::int32_t>(
            BML::IVP::Detail::ByteSwap32(
                static_cast<std::uint32_t>(offset_ledgetree_root)));
    }

    void byte_swap_all(IVP_BOOL swapPoints = IVP_TRUE,
                       int pointEstimate = 100) {
        IVP_Compact_Ledgetree_Node *root =
            const_cast<IVP_Compact_Ledgetree_Node *>(
                get_compact_ledge_tree_root());
        if (root) {
            if (swapPoints) {
                IVP_U_BigVector<IVP_Compact_Poly_Point> swappedPoints(
                    pointEstimate);
                IVP_Compact_Ledge *hull =
                    const_cast<IVP_Compact_Ledge *>(root->get_compact_hull());
                if (hull)
                    hull->byte_swap_all(&swappedPoints);
                root->byte_swap_all(&swappedPoints);
            } else {
                root->byte_swap_all(nullptr);
            }
        }
        byte_swap();
    }

    // Compatibility spellings retained for early BML IVP consumers.
    int get_byte_size() const { return get_size(); }
    const IVP_Compact_Ledgetree_Node *get_ledgetree_root() const {
        return get_compact_ledge_tree_root();
    }
};

class hkMoppCode;

struct IVP_Compact_Mopp {
    IVP_U_Float_Point3 mass_center;
    IVP_U_Float_Point3 rotation_inertia;
    IVP_FLOAT upper_limit_radius;
    union {
        struct {
            unsigned int max_factor_surface_deviation : 8;
            int byte_size : 24;
        };
        std::uint32_t factor_and_size;
    };
    int offset_ledgetree_root;
    int offset_ledges;
    int size_convex_hull;
    int dummy;

    int get_size() const { return byte_size; }

    const hkMoppCode *get_compact_ledge_tree_root() const {
        return reinterpret_cast<const hkMoppCode *>(
            reinterpret_cast<const char *>(this) + offset_ledgetree_root);
    }

    void byte_swap() {
        mass_center.byte_swap();
        rotation_inertia.byte_swap();
        BML::IVP::Detail::ByteSwapFloat(upper_limit_radius);
        const std::uint32_t reordered =
            (factor_and_size << 24u) | (factor_and_size >> 8u);
        factor_and_size = BML::IVP::Detail::ByteSwap32(reordered);
        offset_ledgetree_root = static_cast<int>(
            BML::IVP::Detail::ByteSwap32(
                static_cast<std::uint32_t>(offset_ledgetree_root)));
        offset_ledges = static_cast<int>(
            BML::IVP::Detail::ByteSwap32(
                static_cast<std::uint32_t>(offset_ledges)));
        size_convex_hull = static_cast<int>(
            BML::IVP::Detail::ByteSwap32(
                static_cast<std::uint32_t>(size_convex_hull)));
    }

    // The Ballance image contains neither hkMoppCode's bytecode layout nor a
    // surviving endian walker. Swapping the 0x30 header while leaving the
    // bytecode/ledges untouched would silently corrupt serialized geometry.
    void byte_swap_all(IVP_BOOL swap_points = IVP_TRUE,
                       int point_estimate = 100) = delete;

private:
    IVP_Compact_Mopp() = default;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_U_Hesse) == 0x20);
static_assert(sizeof(IVP_Compact_Poly_Point) == 0x10);
static_assert(sizeof(IVP_Compact_Edge) == 0x04);
static_assert(sizeof(IVP_Compact_Triangle) == 0x10);
static_assert(sizeof(IVP_Compact_Ledge) == 0x10);
static_assert(sizeof(IVP_Compact_Ledgetree_Node) == 0x1C);
static_assert(sizeof(IVP_Compact_Surface) == 0x30);
static_assert(sizeof(IVP_Compact_Mopp) == 0x30);
static_assert(offsetof(IVP_Compact_Surface, rotation_inertia) == 0x0C);
static_assert(offsetof(IVP_Compact_Surface, upper_limit_radius) == 0x18);
static_assert(offsetof(IVP_Compact_Surface, factor_and_size) == 0x1C);
static_assert(offsetof(IVP_Compact_Surface, offset_ledgetree_root) == 0x20);
static_assert(offsetof(IVP_Compact_Surface, reserved) == 0x24);
static_assert(offsetof(IVP_Compact_Mopp, rotation_inertia) == 0x0C);
static_assert(offsetof(IVP_Compact_Mopp, upper_limit_radius) == 0x18);
static_assert(offsetof(IVP_Compact_Mopp, factor_and_size) == 0x1C);
static_assert(offsetof(IVP_Compact_Mopp, offset_ledgetree_root) == 0x20);
static_assert(offsetof(IVP_Compact_Mopp, offset_ledges) == 0x24);
static_assert(offsetof(IVP_Compact_Mopp, size_convex_hull) == 0x28);
static_assert(offsetof(IVP_Compact_Mopp, dummy) == 0x2C);
#endif

#endif // BML_IVP_GEOMETRY_H
