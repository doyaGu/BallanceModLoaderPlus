#ifndef BML_IVP_GRID_H
#define BML_IVP_GRID_H

#include "BML/IVP/Ray.h"
#include "BML/IVP/SurfaceBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

#define IVP_GRID_MAX_ROWS 257
#define IVP_GRID_MAX_COLUMNS IVP_GRID_MAX_ROWS

struct BML_IvpGridBuilderArrayLayoutCheck;

class IVP_Compact_Grid_Element {
public:
    std::int16_t compact_ledge_index[2] = {-1, -1};
};

class IVP_Compact_Grid {
public:
    IVP_U_Float_Hesse center;
    IVP_U_Matrix m_grid_f_object;
    std::int16_t n_rows = 0;
    std::int16_t n_columns = 0;
    int n_compact_ledges = 0;
    IVP_FLOAT radius = 0.0f;
    int byte_size = 0;
    IVP_FLOAT inv_grid_size = 0.0f;
    int offset_grid_elements = 0;
    int offset_compact_ledge_array[1] = {0};

    const IVP_Compact_Grid_Element *get_grid_elements() {
        return reinterpret_cast<const IVP_Compact_Grid_Element *>(
            reinterpret_cast<const std::byte *>(this) + offset_grid_elements);
    }

    const IVP_Compact_Ledge *get_compact_ledge_at(int index) {
        return reinterpret_cast<const IVP_Compact_Ledge *>(
            reinterpret_cast<const std::byte *>(this) +
            offset_compact_ledge_array[index]);
    }
};

class IVP_Template_Grid_Axle_Descript {
public:
    int n_points = 0;
    IVP_COORDINATE_INDEX maps_to = IVP_INDEX_X;
    IVP_BOOL invert_axis = IVP_FALSE;
};

class IVP_Template_Compact_Grid {
public:
    IVP_Template_Grid_Axle_Descript row_info;
    IVP_Template_Grid_Axle_Descript column_info;
    IVP_COORDINATE_INDEX height_maps_to = IVP_INDEX_X;
    IVP_BOOL height_invert_axis = IVP_FALSE;
    IVP_FLOAT grid_field_size = 0.0f;
    IVP_U_Float_Point position_origin_os;

    IVP_Template_Compact_Grid() { std::memset(this, 0, sizeof(*this)); }
};

class IVP_GridBuilder_Array {
    friend struct BML_IvpGridBuilderArrayLayoutCheck;

public:
    static IVP_Compact_Grid *convert_array_to_compact_grid(
        IVP_Environment *, const IVP_Template_Compact_Grid *parameters,
        IVP_FLOAT *heightField) {
        if (!parameters || !heightField || parameters->grid_field_size <= 0.0f)
            return nullptr;

        const int pointRows = parameters->row_info.n_points;
        const int pointColumns = parameters->column_info.n_points;
        if (pointRows < 2 || pointColumns < 2 ||
            pointRows > IVP_GRID_MAX_ROWS ||
            pointColumns > IVP_GRID_MAX_COLUMNS)
            return nullptr;

        const int rowAxis = static_cast<int>(parameters->row_info.maps_to);
        const int columnAxis =
            static_cast<int>(parameters->column_info.maps_to);
        const int heightAxis = static_cast<int>(parameters->height_maps_to);
        if (rowAxis < 0 || rowAxis > 2 || columnAxis < 0 ||
            columnAxis > 2 || heightAxis < 0 || heightAxis > 2 ||
            rowAxis == columnAxis || rowAxis == heightAxis ||
            columnAxis == heightAxis)
            return nullptr;

        const int cellRows = pointRows - 1;
        const int cellColumns = pointColumns - 1;
        const std::size_t cellCount =
            static_cast<std::size_t>(cellRows) * cellColumns;
        const std::size_t ledgeCount = cellCount * 2u;
        if (ledgeCount >
            static_cast<std::size_t>((std::numeric_limits<std::int16_t>::max)()))
            return nullptr;

        struct LedgeOwner {
            IVP_Compact_Ledge *ledge = nullptr;
            ~LedgeOwner() {
                if (ledge)
                    BML::IVP::ABI::Invoke<void>(
                        BML::IVP::ABI::Address::FreeAligned, ledge);
            }
            LedgeOwner() = default;
            explicit LedgeOwner(IVP_Compact_Ledge *value) : ledge(value) {}
            LedgeOwner(const LedgeOwner &) = delete;
            LedgeOwner &operator=(const LedgeOwner &) = delete;
            LedgeOwner(LedgeOwner &&other) noexcept : ledge(other.ledge) {
                other.ledge = nullptr;
            }
            LedgeOwner &operator=(LedgeOwner &&other) noexcept {
                if (this == &other)
                    return *this;
                if (ledge)
                    BML::IVP::ABI::Invoke<void>(
                        BML::IVP::ABI::Address::FreeAligned, ledge);
                ledge = other.ledge;
                other.ledge = nullptr;
                return *this;
            }
        };

        const IVP_FLOAT rowStep = parameters->row_info.invert_axis
            ? -parameters->grid_field_size
            : parameters->grid_field_size;
        const IVP_FLOAT columnStep = parameters->column_info.invert_axis
            ? -parameters->grid_field_size
            : parameters->grid_field_size;
        const IVP_FLOAT heightSign = parameters->height_invert_axis
            ? -1.0f
            : 1.0f;
        auto pointAt = [&](int row, int column) {
            IVP_U_Point point;
            point.set(&parameters->position_origin_os);
            point.k[rowAxis] += static_cast<IVP_DOUBLE>(rowStep) * row;
            point.k[columnAxis] +=
                static_cast<IVP_DOUBLE>(columnStep) * column;
            point.k[heightAxis] += static_cast<IVP_DOUBLE>(heightSign) *
                heightField[row * pointColumns + column];
            return point;
        };

        std::vector<LedgeOwner> ledges;
        ledges.reserve(ledgeCount);
        for (int row = 0; row < cellRows; ++row) {
            for (int column = 0; column < cellColumns; ++column) {
                IVP_U_Point point00 = pointAt(row, column);
                IVP_U_Point point10 = pointAt(row + 1, column);
                IVP_U_Point point01 = pointAt(row, column + 1);
                IVP_U_Point point11 = pointAt(row + 1, column + 1);
                IVP_Compact_Ledge *first =
                    IVP_SurfaceBuilder_Pointsoup::
                        convert_triangle_to_compace_ledge(
                            &point00, &point10, &point11);
                if (!first)
                    return nullptr;
                ledges.emplace_back(first);
                IVP_Compact_Ledge *second =
                    IVP_SurfaceBuilder_Pointsoup::
                        convert_triangle_to_compace_ledge(
                            &point00, &point11, &point01);
                if (!second)
                    return nullptr;
                ledges.emplace_back(second);
            }
        }

        auto align16 = [](std::size_t value) {
            return (value + 15u) & ~std::size_t{15u};
        };
        const std::size_t offsetArray =
            offsetof(IVP_Compact_Grid, offset_compact_ledge_array);
        const std::size_t elementsOffset =
            align16(offsetArray + ledgeCount * sizeof(int));
        const std::size_t ledgesOffset = align16(
            elementsOffset + cellCount * sizeof(IVP_Compact_Grid_Element));
        std::size_t byteSize = ledgesOffset;
        for (const LedgeOwner &owner : ledges)
            byteSize += static_cast<std::size_t>(owner.ledge->get_size());
        if (byteSize > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            return nullptr;

        auto *grid = static_cast<IVP_Compact_Grid *>(
            BML::IVP::ABI::Invoke<void *>(
                BML::IVP::ABI::Address::AllocateAligned,
                static_cast<int>(byteSize + 4u), 16));
        if (!grid)
            return nullptr;
        std::memset(grid, 0, byteSize + 4u);
        grid->n_rows = static_cast<std::int16_t>(cellRows);
        grid->n_columns = static_cast<std::int16_t>(cellColumns);
        grid->n_compact_ledges = static_cast<int>(ledgeCount);
        grid->byte_size = static_cast<int>(byteSize);
        grid->inv_grid_size = 1.0f / parameters->grid_field_size;
        grid->offset_grid_elements = static_cast<int>(elementsOffset);

        IVP_U_Point rowVector;
        IVP_U_Point columnVector;
        IVP_U_Point heightVector;
        rowVector.set_to_zero();
        columnVector.set_to_zero();
        heightVector.set_to_zero();
        rowVector.k[rowAxis] = rowStep;
        columnVector.k[columnAxis] = columnStep;
        heightVector.k[heightAxis] = heightSign;
        IVP_U_Matrix objectFromGrid;
        IVP_U_Point origin(parameters->position_origin_os);
        objectFromGrid.init_columns4(
            &rowVector, &columnVector, &heightVector,
            &origin);
        if (grid->m_grid_f_object.real_invert(&objectFromGrid) != IVP_OK) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::FreeAligned, grid);
            return nullptr;
        }

        IVP_U_Point minimum = pointAt(0, 0);
        IVP_U_Point maximum = minimum;
        for (int row = 0; row < pointRows; ++row) {
            for (int column = 0; column < pointColumns; ++column) {
                const IVP_U_Point point = pointAt(row, column);
                for (int axis = 0; axis < 3; ++axis) {
                    minimum.k[axis] = (std::min)(minimum.k[axis], point.k[axis]);
                    maximum.k[axis] = (std::max)(maximum.k[axis], point.k[axis]);
                }
            }
        }
        IVP_U_Point center;
        center.set_interpolate(&minimum, &maximum, 0.5);
        grid->center.set(&center);
        IVP_DOUBLE squaredRadius = 0.0;
        for (int row = 0; row < pointRows; ++row) {
            for (int column = 0; column < pointColumns; ++column) {
                const IVP_U_Point point = pointAt(row, column);
                squaredRadius = (std::max)(
                    squaredRadius, center.quad_distance_to(&point));
            }
        }
        grid->radius = static_cast<IVP_FLOAT>(std::sqrt(squaredRadius));

        auto *elements = const_cast<IVP_Compact_Grid_Element *>(
            grid->get_grid_elements());
        std::size_t destinationOffset = ledgesOffset;
        for (std::size_t index = 0; index < ledgeCount; ++index) {
            const int ledgeSize = ledges[index].ledge->get_size();
            grid->offset_compact_ledge_array[index] =
                static_cast<int>(destinationOffset);
            std::memcpy(
                reinterpret_cast<std::byte *>(grid) + destinationOffset,
                ledges[index].ledge, static_cast<std::size_t>(ledgeSize));
            destinationOffset += static_cast<std::size_t>(ledgeSize);
        }
        for (std::size_t cell = 0; cell < cellCount; ++cell) {
            elements[cell].compact_ledge_index[0] =
                static_cast<std::int16_t>(cell * 2u);
            elements[cell].compact_ledge_index[1] =
                static_cast<std::int16_t>(cell * 2u + 1u);
        }
        return grid;
    }

    ~IVP_GridBuilder_Array() = default;

private:
    IVP_GridBuilder_Array() = delete;
    int n_rows;
    int n_cols;
    IVP_FLOAT *height_field;
    IVP_Compact_Poly_Point *height_points;
    IVP_BOOL is_left_handed;
    IVP_Compact_Grid_Element *ledge_reference_field;
    const IVP_Template_Compact_Grid *templ;
    IVP_U_Memory *mm;
    int *grid_point_to_ledge_point_array;
    IVP_Compact_Poly_Point *compact_poly_point_buffer;
    int n_compact_poly_points_used;
    IVP_Compact_Ledge *c_ledge;
    IVP_Compact_Poly_Point *c_points;
    std::uint16_t c_point_to_point_index[IVP_GRID_MAX_ROWS * 2 + 2];
    int triangle_count;
};

class IVP_SurfaceManager_Grid : public IVP_SurfaceManager {
protected:
    IVP_Compact_Grid *compact_grid;

    IVP_SurfaceManager_Grid() : compact_grid(nullptr) {}

    void traverse_grid(
        const IVP_U_Point &visitorPositionObject, IVP_DOUBLE radius,
        IVP_U_BigVector<IVP_Compact_Ledge> *result) const {
        if (!compact_grid || !result || radius < 0.0 ||
            compact_grid->n_rows <= 0 || compact_grid->n_columns <= 0 ||
            compact_grid->n_compact_ledges <= 0)
            return;

        IVP_U_Point visitorGrid;
        compact_grid->m_grid_f_object.vmult4(
            &visitorPositionObject, &visitorGrid);
        const IVP_DOUBLE radiusGrid =
            radius * compact_grid->inv_grid_size;
        IVP_DOUBLE minRow = visitorGrid.k[0] - radiusGrid;
        IVP_DOUBLE maxRow = visitorGrid.k[0] + radiusGrid;
        IVP_DOUBLE minColumn = visitorGrid.k[1] - radiusGrid;
        IVP_DOUBLE maxColumn = visitorGrid.k[1] + radiusGrid;
        minRow = (std::max)(minRow, 0.0);
        minColumn = (std::max)(minColumn, 0.0);
        maxRow = (std::min)(
            maxRow, static_cast<IVP_DOUBLE>(compact_grid->n_rows) - 0.5);
        maxColumn = (std::min)(
            maxColumn,
            static_cast<IVP_DOUBLE>(compact_grid->n_columns) - 0.5);
        if (minRow > maxRow || minColumn > maxColumn)
            return;

        std::vector<std::uint8_t> seen(
            static_cast<std::size_t>(compact_grid->n_compact_ledges), 0);
        const IVP_Compact_Grid_Element *elements =
            compact_grid->get_grid_elements();
        for (int row = static_cast<int>(minRow);
             row <= static_cast<int>(maxRow); ++row) {
            for (int column = static_cast<int>(minColumn);
                 column <= static_cast<int>(maxColumn); ++column) {
                const IVP_Compact_Grid_Element &element =
                    elements[row * compact_grid->n_columns + column];
                for (int slot = 0; slot < 2; ++slot) {
                    const int ledgeIndex = element.compact_ledge_index[slot];
                    if (ledgeIndex < 0 ||
                        ledgeIndex >= compact_grid->n_compact_ledges ||
                        seen[static_cast<std::size_t>(ledgeIndex)] != 0)
                        continue;
                    seen[static_cast<std::size_t>(ledgeIndex)] = 1;
                    const IVP_Compact_Ledge *ledge =
                        compact_grid->get_compact_ledge_at(ledgeIndex);
                    if (ledge->get_n_triangles() == 2) {
                        const IVP_DOUBLE squaredDistance =
                            BML::IVP::ABI::Invoke<IVP_DOUBLE>(
                                BML::IVP::ABI::Address::CompactLedgeSolverPointTriangleDistance,
                                ledge, ledge->get_first_triangle(),
                                &visitorPositionObject);
                        if (squaredDistance > radius * radius)
                            continue;
                    }
                    result->add(const_cast<IVP_Compact_Ledge *>(ledge));
                }
            }
        }
    }

public:
    explicit IVP_SurfaceManager_Grid(IVP_Compact_Grid *grid)
        : compact_grid(grid) {}

    void add_reference_to_ledge(const IVP_Compact_Ledge *) override {}
    void remove_reference_to_ledge(const IVP_Compact_Ledge *) override {}

    void insert_all_ledges_hitting_ray(
        IVP_Ray_Solver *raySolver, IVP_Real_Object *object) override {
        if (!raySolver || !object)
            return;
        IVP_Vector_of_Ledges_256 ledges;
        IVP_Ray_Solver_Os objectRay(raySolver, object);
        IVP_U_Point center(objectRay.ray_center_point);
        get_all_ledges_within_radius(
            &center, objectRay.ray_length * 0.5f, nullptr, nullptr,
            nullptr, &ledges);
        for (int index = ledges.len() - 1; index >= 0; --index)
            objectRay.check_ray_against_compact_ledge_os(
                ledges.element_at(index));
    }

    void get_radius_and_radius_dev_to_given_center(
        const IVP_U_Float_Point *, IVP_FLOAT *radius,
        IVP_FLOAT *radiusDeviation) const override {
        if (radius)
            *radius = compact_grid ? compact_grid->radius : 0.0f;
        if (radiusDeviation)
            *radiusDeviation = compact_grid ? compact_grid->radius : 0.0f;
    }

    IVP_SURMAN_TYPE get_type() override { return IVP_SURMAN_POLYGON; }
    const IVP_Compact_Ledge *get_single_convex() const override {
        return nullptr;
    }
    void get_mass_center(IVP_U_Float_Point *output) const override {
        if (output) {
            if (compact_grid)
                output->set(compact_grid->center.k);
            else
                output->set_to_zero();
        }
    }
    void get_rotation_inertia(IVP_U_Float_Point *output) const override {
        if (!output)
            return;
        const IVP_FLOAT squaredRadius = compact_grid
            ? compact_grid->radius * compact_grid->radius
            : 0.0f;
        output->set(squaredRadius, squaredRadius, squaredRadius);
    }
    void get_all_ledges_within_radius(
        const IVP_U_Point *observerPositionObject, IVP_DOUBLE radius,
        const IVP_Compact_Ledge *, IVP_Real_Object *,
        const IVP_Compact_Ledge *,
        IVP_U_BigVector<IVP_Compact_Ledge> *result) override {
        if (observerPositionObject)
            traverse_grid(*observerPositionObject, radius, result);
    }
    void get_all_terminal_ledges(
        IVP_U_BigVector<IVP_Compact_Ledge> *result) override {
        IVP_U_Point origin;
        origin.set_to_zero();
        traverse_grid(origin, (std::numeric_limits<IVP_DOUBLE>::max)(), result);
    }
    const IVP_Compact_Grid *get_compact_grid() const { return compact_grid; }
    ~IVP_SurfaceManager_Grid() override = default;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Compact_Grid_Element) == 0x04);
static_assert(sizeof(IVP_Compact_Grid) == 0xB0);
static_assert(offsetof(IVP_Compact_Grid, m_grid_f_object) == 0x10);
static_assert(offsetof(IVP_Compact_Grid, n_rows) == 0x90);
static_assert(offsetof(IVP_Compact_Grid, n_columns) == 0x92);
static_assert(offsetof(IVP_Compact_Grid, n_compact_ledges) == 0x94);
static_assert(offsetof(IVP_Compact_Grid, radius) == 0x98);
static_assert(offsetof(IVP_Compact_Grid, byte_size) == 0x9C);
static_assert(offsetof(IVP_Compact_Grid, inv_grid_size) == 0xA0);
static_assert(offsetof(IVP_Compact_Grid, offset_grid_elements) == 0xA4);
static_assert(
    offsetof(IVP_Compact_Grid, offset_compact_ledge_array) == 0xA8);
static_assert(sizeof(IVP_Template_Grid_Axle_Descript) == 0x0C);
static_assert(sizeof(IVP_Template_Compact_Grid) == 0x34);
static_assert(offsetof(IVP_Template_Compact_Grid, column_info) == 0x0C);
static_assert(offsetof(IVP_Template_Compact_Grid, height_maps_to) == 0x18);
static_assert(
    offsetof(IVP_Template_Compact_Grid, height_invert_axis) == 0x1C);
static_assert(offsetof(IVP_Template_Compact_Grid, grid_field_size) == 0x20);
static_assert(
    offsetof(IVP_Template_Compact_Grid, position_origin_os) == 0x24);
static_assert(sizeof(IVP_GridBuilder_Array) == 0x440);
static_assert(sizeof(IVP_SurfaceManager_Grid) == 0x08);

struct BML_IvpGridBuilderArrayLayoutCheck {
    static_assert(offsetof(IVP_GridBuilder_Array, n_rows) == 0x00);
    static_assert(offsetof(IVP_GridBuilder_Array, height_field) == 0x08);
    static_assert(
        offsetof(IVP_GridBuilder_Array, ledge_reference_field) == 0x14);
    static_assert(offsetof(IVP_GridBuilder_Array, templ) == 0x18);
    static_assert(
        offsetof(IVP_GridBuilder_Array,
                 grid_point_to_ledge_point_array) == 0x20);
    static_assert(
        offsetof(IVP_GridBuilder_Array, n_compact_poly_points_used) == 0x28);
    static_assert(offsetof(IVP_GridBuilder_Array, c_ledge) == 0x2C);
    static_assert(
        offsetof(IVP_GridBuilder_Array, c_point_to_point_index) == 0x34);
    static_assert(offsetof(IVP_GridBuilder_Array, triangle_count) == 0x43C);
};
#endif

#endif // BML_IVP_GRID_H
