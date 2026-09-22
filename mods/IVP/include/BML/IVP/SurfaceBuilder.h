#ifndef BML_IVP_SURFACE_BUILDER_H
#define BML_IVP_SURFACE_BUILDER_H

#include "BML/IVP/Surface.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <new>
#include <utility>
#include <vector>

enum IVP_SURBUILD_LEDGE_SOUP_MERGE_POINT_TYPES : std::int32_t {
    IVP_SLMP_NO_MERGE = 0,
    IVP_SLMP_MERGE_AND_REALLOCATE = 1,
    IVP_SLMP_MERGE_NO_REALLOCATE = 2,
};

struct BML_IvpSurfaceBuilderQ12LayoutCheck;
struct BML_IvpSurfaceBuilderLedgeSoupLayoutCheck;

struct IVV_Sphere_Cluster;
class IVV_Sphere;
class IVV_Cluster_Min_Hash;
class IVP_I_FPoint_VHash;

class IVP_Concave_Polyhedron_Face_Pointoffset {
public:
    int offset = 0;
};

class IVP_Concave_Polyhedron_Face {
public:
    IVP_U_Vector<IVP_Concave_Polyhedron_Face_Pointoffset> point_offset;

    void add_offset(intp offset) {
        for (int index = 0; index < point_offset.len(); ++index) {
            if (point_offset.element_at(index)->offset == offset)
                return;
        }
        auto *entry = new IVP_Concave_Polyhedron_Face_Pointoffset;
        entry->offset = static_cast<int>(offset);
        point_offset.add(entry);
    }

    ~IVP_Concave_Polyhedron_Face() {
        for (int index = point_offset.len() - 1; index >= 0; --index)
            delete point_offset.element_at(index);
    }
};

class IVP_Concave_Polyhedron {
public:
    IVP_U_BigVector<IVP_U_Point> points;
    IVP_U_BigVector<IVP_Concave_Polyhedron_Face> faces;
};

class IVP_Template_SurfaceBuilder_3ds {
public:
    IVP_FLOAT scale;

    IVP_Template_SurfaceBuilder_3ds() : scale(1.0f) {}
};

namespace BML::IVP::Detail {

struct ThreeDsMesh {
    std::vector<IVP_U_Point> points;
    std::vector<std::array<std::uint16_t, 3>> faces;
};

class ThreeDsReader {
public:
    explicit ThreeDsReader(std::vector<std::byte> bytes)
        : bytes_(std::move(bytes)) {}

    bool Parse(std::vector<ThreeDsMesh> &meshes) const {
        if (bytes_.size() < 6 || Read16(0) != 0x4D4D)
            return false;
        const std::uint32_t rootLength = Read32(2);
        if (rootLength < 6 || rootLength > bytes_.size())
            return false;
        return ParseChunks(6, rootLength, nullptr, meshes);
    }

private:
    bool CanRead(std::size_t offset, std::size_t size) const {
        return offset <= bytes_.size() && size <= bytes_.size() - offset;
    }

    std::uint16_t Read16(std::size_t offset) const {
        std::uint16_t value = 0;
        if (CanRead(offset, sizeof(value)))
            std::memcpy(&value, bytes_.data() + offset, sizeof(value));
        return value;
    }

    std::uint32_t Read32(std::size_t offset) const {
        std::uint32_t value = 0;
        if (CanRead(offset, sizeof(value)))
            std::memcpy(&value, bytes_.data() + offset, sizeof(value));
        return value;
    }

    IVP_FLOAT ReadFloat(std::size_t offset) const {
        IVP_FLOAT value = 0.0f;
        if (CanRead(offset, sizeof(value)))
            std::memcpy(&value, bytes_.data() + offset, sizeof(value));
        return value;
    }

    bool ParseVertices(std::size_t begin, std::size_t end,
                       ThreeDsMesh &mesh) const {
        if (!CanRead(begin, 2))
            return false;
        const std::size_t count = Read16(begin);
        if (count > (end - begin - 2) / 12)
            return false;
        mesh.points.clear();
        mesh.points.reserve(count);
        std::size_t cursor = begin + 2;
        for (std::size_t index = 0; index < count; ++index) {
            const IVP_FLOAT x = ReadFloat(cursor);
            const IVP_FLOAT y = ReadFloat(cursor + 4);
            const IVP_FLOAT z = ReadFloat(cursor + 8);
            if (!std::isfinite(x) || !std::isfinite(y) ||
                !std::isfinite(z))
                return false;
            mesh.points.emplace_back(x, y, z);
            cursor += 12;
        }
        return true;
    }

    bool ParseFaces(std::size_t begin, std::size_t end,
                    ThreeDsMesh &mesh) const {
        if (!CanRead(begin, 2))
            return false;
        const std::size_t count = Read16(begin);
        if (count > (end - begin - 2) / 8)
            return false;
        mesh.faces.clear();
        mesh.faces.reserve(count);
        std::size_t cursor = begin + 2;
        for (std::size_t index = 0; index < count; ++index) {
            mesh.faces.push_back({Read16(cursor), Read16(cursor + 2),
                                  Read16(cursor + 4)});
            cursor += 8; // the fourth word contains 3DS edge flags
        }
        return true;
    }

    bool ParseChunks(std::size_t begin, std::size_t end,
                     ThreeDsMesh *mesh,
                     std::vector<ThreeDsMesh> &meshes) const {
        if (end > bytes_.size() || begin > end)
            return false;
        std::size_t cursor = begin;
        while (cursor < end) {
            if (end - cursor < 6)
                return false;
            const std::uint16_t id = Read16(cursor);
            const std::uint32_t length = Read32(cursor + 2);
            if (length < 6 || length > end - cursor)
                return false;
            const std::size_t payload = cursor + 6;
            const std::size_t chunkEnd = cursor + length;
            switch (id) {
            case 0x3D3D: // object mesh container
                if (!ParseChunks(payload, chunkEnd, nullptr, meshes))
                    return false;
                break;
            case 0x4000: { // named object container
                std::size_t child = payload;
                while (child < chunkEnd && bytes_[child] != std::byte{0})
                    ++child;
                if (child == chunkEnd ||
                    !ParseChunks(child + 1, chunkEnd, nullptr, meshes))
                    return false;
                break;
            }
            case 0x4100: // triangular mesh container
                meshes.emplace_back();
                if (!ParseChunks(payload, chunkEnd, &meshes.back(), meshes))
                    return false;
                break;
            case 0x4110: // vertex list
                if (!mesh || !ParseVertices(payload, chunkEnd, *mesh))
                    return false;
                break;
            case 0x4120: // face list (material subchunks may follow)
                if (!mesh || !ParseFaces(payload, chunkEnd, *mesh))
                    return false;
                break;
            default:
                break;
            }
            cursor = chunkEnd;
        }
        return cursor == end;
    }

    std::vector<std::byte> bytes_;
};

inline bool PointOnOpenEdge(const IVP_U_Point &point,
                            const IVP_U_Point &start,
                            const IVP_U_Point &end) {
    IVP_U_Point edge;
    IVP_U_Point relative;
    edge.subtract(&end, &start);
    relative.subtract(&point, &start);
    const IVP_DOUBLE edgeLengthSquared = edge.quad_length();
    if (edgeLengthSquared <= 1.0e-12)
        return false;
    const IVP_DOUBLE position = relative.dot_product(&edge) / edgeLengthSquared;
    if (position <= 0.0 || position >= 1.0)
        return false;
    IVP_U_Point nearest(edge);
    nearest.mult(position);
    nearest.add(&start);
    return nearest.quad_distance_to(&point) < 1.0e-6;
}

inline void RepairThreeDsTJunctions(IVP_Concave_Polyhedron &polyhedron) {
    for (int pointIndex = 0; pointIndex < polyhedron.points.len();
         ++pointIndex) {
        IVP_U_Point *point = polyhedron.points.element_at(pointIndex);
        for (int faceIndex = 0; faceIndex < polyhedron.faces.len();
             ++faceIndex) {
            IVP_Concave_Polyhedron_Face *face =
                polyhedron.faces.element_at(faceIndex);
            for (int edgeIndex = 0; edgeIndex < face->point_offset.len();
                 ++edgeIndex) {
                const int nextIndex =
                    (edgeIndex + 1) % face->point_offset.len();
                const int start =
                    face->point_offset.element_at(edgeIndex)->offset;
                const int end = face->point_offset.element_at(nextIndex)->offset;
                if (pointIndex == start || pointIndex == end)
                    continue;
                if (PointOnOpenEdge(*point,
                                    *polyhedron.points.element_at(start),
                                    *polyhedron.points.element_at(end))) {
                    auto *entry =
                        new IVP_Concave_Polyhedron_Face_Pointoffset;
                    entry->offset = pointIndex;
                    face->point_offset.insert_after(edgeIndex, entry);
                    ++edgeIndex;
                }
            }
        }
    }
}

} // namespace BML::IVP::Detail

class IVP_SurfaceBuilder_3ds {
public:
    static IVP_Concave_Polyhedron *convert_3ds_to_concave(
        const char *filename, IVP_Template_SurfaceBuilder_3ds *parameters) {
        if (!filename || !parameters || !std::isfinite(parameters->scale))
            return nullptr;
        std::ifstream input(filename, std::ios::binary | std::ios::ate);
        if (!input)
            return nullptr;
        const std::streamoff length = input.tellg();
        if (length < 6 ||
            static_cast<std::uintmax_t>(length) >
                std::numeric_limits<std::size_t>::max())
            return nullptr;
        std::vector<std::byte> bytes(static_cast<std::size_t>(length));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char *>(bytes.data()), length))
            return nullptr;

        std::vector<BML::IVP::Detail::ThreeDsMesh> meshes;
        if (!BML::IVP::Detail::ThreeDsReader(std::move(bytes)).Parse(meshes) ||
            meshes.empty())
            return nullptr;

        auto *polyhedron = new IVP_Concave_Polyhedron;
        for (const auto &mesh : meshes) {
            std::vector<int> pointMap(mesh.points.size(), -1);
            for (std::size_t localIndex = 0; localIndex < mesh.points.size();
                 ++localIndex) {
                // The neighboring converter scales H3dsVert float fields in
                // place before widening them to IVP_U_Point. Preserve that
                // rounding boundary rather than multiplying in double.
                IVP_U_Point scaled(
                    static_cast<IVP_FLOAT>(
                        mesh.points[localIndex].k[0] * parameters->scale),
                    static_cast<IVP_FLOAT>(
                        mesh.points[localIndex].k[1] * parameters->scale),
                    static_cast<IVP_FLOAT>(
                        mesh.points[localIndex].k[2] * parameters->scale));
                int globalIndex = 0;
                for (; globalIndex < polyhedron->points.len(); ++globalIndex) {
                    const IVP_U_Point *old =
                        polyhedron->points.element_at(globalIndex);
                    if (old->k[0] == scaled.k[0] &&
                        old->k[1] == scaled.k[1] &&
                        old->k[2] == scaled.k[2])
                        break;
                }
                if (globalIndex == polyhedron->points.len())
                    polyhedron->points.add(new IVP_U_Point(scaled));
                pointMap[localIndex] = globalIndex;
            }
            for (const auto &indices : mesh.faces) {
                if (indices[0] >= pointMap.size() ||
                    indices[1] >= pointMap.size() ||
                    indices[2] >= pointMap.size()) {
                    Destroy(polyhedron);
                    return nullptr;
                }
                auto *face = new IVP_Concave_Polyhedron_Face;
                face->add_offset(pointMap[indices[0]]);
                face->add_offset(pointMap[indices[1]]);
                face->add_offset(pointMap[indices[2]]);
                if (face->point_offset.len() < 3) {
                    delete face;
                    continue;
                }
                polyhedron->faces.add(face);
            }
        }
        if (polyhedron->points.len() == 0 || polyhedron->faces.len() == 0) {
            Destroy(polyhedron);
            return nullptr;
        }
        BML::IVP::Detail::RepairThreeDsTJunctions(*polyhedron);
        return polyhedron;
    }

private:
    static void Destroy(IVP_Concave_Polyhedron *polyhedron) {
        for (int index = polyhedron->faces.len() - 1; index >= 0; --index)
            delete polyhedron->faces.element_at(index);
        for (int index = polyhedron->points.len() - 1; index >= 0; --index)
            delete polyhedron->points.element_at(index);
        delete polyhedron;
    }
};

#ifndef MAX_MAP_HULLS
#define MAX_MAP_HULLS 4
#endif
#ifndef HEADER_LUMPS
#define HEADER_LUMPS 15
#endif

struct dmodel_t {
    IVP_FLOAT mins[3];
    IVP_FLOAT maxs[3];
    IVP_FLOAT origin[3];
    intp headnode[MAX_MAP_HULLS];
    int visleafs;
    int firstface;
    int numfaces;
};

struct lump_t {
    int fileofs;
    int filelen;
};

struct dheader_t {
    int version;
    lump_t lumps[HEADER_LUMPS];
};

struct dplane_t {
    IVP_FLOAT normal[3];
    IVP_FLOAT dist;
    int type;
};

struct dnode_t {
    int planenum;
    short children[2];
    short mins[3];
    short maxs[3];
    unsigned short firstface;
    unsigned short numfaces;
};

struct dclipnode_t {
    int planenum;
    short children[2];
};

class IVP_q12_int {
public:
    int val;

    explicit IVP_q12_int(int value) : val(value) {}
};

class IVP_Convex_Subpart {
public:
    IVP_U_Vector<IVP_U_Point> points;

    ~IVP_Convex_Subpart() {
        for (int index = points.len() - 1; index >= 0; --index)
            delete points.element_at(index);
    }
};

class IVP_Convex_Decompositor_Parameters {
public:
    IVP_FLOAT tolin = 0.0f;
    IVP_FLOAT angacc = 0.0f;
    IVP_FLOAT rdacc = 0.0f;
};

class IVP_Convex_Decompositor {
public:
    static int perform_convex_decomposition_on_concave_polyhedron(
        IVP_Concave_Polyhedron *polyhedron,
        IVP_Convex_Decompositor_Parameters *parameters,
        IVP_U_BigVector<IVP_Convex_Subpart> *subparts) {
        if (!polyhedron || !subparts || polyhedron->points.len() < 4 ||
            polyhedron->faces.len() < 4)
            return 0;

        const IVP_DOUBLE tolerance =
            parameters && parameters->tolin > 0.0f
                ? parameters->tolin
                : 1.0e-6;
        IVP_U_Point kernel;
        kernel.set_to_zero();
        for (int index = 0; index < polyhedron->points.len(); ++index) {
            IVP_U_Point *point = polyhedron->points.element_at(index);
            if (!point)
                return 0;
            kernel.add(point);
        }
        kernel.mult(1.0 / polyhedron->points.len());

        struct FacePlane {
            IVP_U_Point normal;
            IVP_DOUBLE offset = 0.0;
        };
        std::vector<FacePlane> planes;
        planes.reserve(static_cast<std::size_t>(polyhedron->faces.len()));
        std::vector<std::pair<int, int>> directedEdges;
        bool isConvex = true;
        for (int faceIndex = 0; faceIndex < polyhedron->faces.len();
             ++faceIndex) {
            IVP_Concave_Polyhedron_Face *face =
                polyhedron->faces.element_at(faceIndex);
            if (!face || face->point_offset.len() < 3)
                return 0;
            for (int pointIndex = 0; pointIndex < face->point_offset.len();
                 ++pointIndex) {
                const int offset = face->point_offset.element_at(pointIndex)->offset;
                if (offset < 0 || offset >= polyhedron->points.len())
                    return 0;
            }
            const IVP_U_Point *point0 = polyhedron->points.element_at(
                face->point_offset.element_at(0)->offset);
            const IVP_U_Point *point1 = polyhedron->points.element_at(
                face->point_offset.element_at(1)->offset);
            const IVP_U_Point *point2 = polyhedron->points.element_at(
                face->point_offset.element_at(2)->offset);
            IVP_U_Point edge0;
            IVP_U_Point edge1;
            FacePlane plane;
            edge0.subtract(point1, point0);
            edge1.subtract(point2, point0);
            plane.normal.inline_calc_cross_product(&edge0, &edge1);
            const IVP_DOUBLE normalLength = plane.normal.real_length();
            if (normalLength <= tolerance)
                return 0;
            plane.normal.mult(1.0 / normalLength);
            plane.offset = -plane.normal.dot_product(point0);
            // Public input faces are counter-clockwise when viewed from the
            // outside. A kernel point must therefore lie behind every face.
            if (plane.normal.dot_product(&kernel) + plane.offset > tolerance)
                return 0;
            for (int pointIndex = 0; pointIndex < face->point_offset.len();
                 ++pointIndex) {
                const int currentOffset =
                    face->point_offset.element_at(pointIndex)->offset;
                const int nextOffset = face->point_offset.element_at(
                    (pointIndex + 1) % face->point_offset.len())->offset;
                directedEdges.emplace_back(currentOffset, nextOffset);
                const IVP_U_Point *facePoint =
                    polyhedron->points.element_at(currentOffset);
                if (std::abs(plane.normal.dot_product(facePoint) +
                             plane.offset) > tolerance)
                    return 0;

                const IVP_U_Point *nextPoint =
                    polyhedron->points.element_at(nextOffset);
                const IVP_U_Point *followingPoint =
                    polyhedron->points.element_at(
                        face->point_offset.element_at(
                            (pointIndex + 2) % face->point_offset.len())->offset);
                IVP_U_Point polygonEdge0;
                IVP_U_Point polygonEdge1;
                IVP_U_Point polygonTurn;
                polygonEdge0.subtract(nextPoint, facePoint);
                polygonEdge1.subtract(followingPoint, nextPoint);
                polygonTurn.inline_calc_cross_product(
                    &polygonEdge0, &polygonEdge1);
                if (polygonTurn.dot_product(&plane.normal) < -tolerance)
                    return 0;
            }
            for (int pointIndex = 0; pointIndex < polyhedron->points.len();
                 ++pointIndex) {
                if (plane.normal.dot_product(
                        polyhedron->points.element_at(pointIndex)) +
                        plane.offset > tolerance) {
                    isConvex = false;
                    break;
                }
            }
            planes.push_back(plane);
        }
        for (std::size_t edgeIndex = 0; edgeIndex < directedEdges.size();
             ++edgeIndex) {
            int reverseCount = 0;
            int duplicateCount = 0;
            for (std::size_t otherIndex = 0;
                 otherIndex < directedEdges.size(); ++otherIndex) {
                if (edgeIndex == otherIndex)
                    continue;
                if (directedEdges[otherIndex].first ==
                        directedEdges[edgeIndex].second &&
                    directedEdges[otherIndex].second ==
                        directedEdges[edgeIndex].first)
                    ++reverseCount;
                if (directedEdges[otherIndex] == directedEdges[edgeIndex])
                    ++duplicateCount;
            }
            if (reverseCount != 1 || duplicateCount != 0)
                return 0;
        }

        if (isConvex) {
            auto *subpart = new IVP_Convex_Subpart;
            for (int index = 0; index < polyhedron->points.len(); ++index)
                subpart->points.add(
                    new IVP_U_Point(*polyhedron->points.element_at(index)));
            subparts->add(subpart);
            return 1;
        }

        int created = 0;
        for (int faceIndex = 0; faceIndex < polyhedron->faces.len();
             ++faceIndex) {
            IVP_Concave_Polyhedron_Face *face =
                polyhedron->faces.element_at(faceIndex);
            const IVP_U_Point *point0 = polyhedron->points.element_at(
                face->point_offset.element_at(0)->offset);
            for (int triangle = 1;
                 triangle + 1 < face->point_offset.len(); ++triangle) {
                const IVP_U_Point *point1 = polyhedron->points.element_at(
                    face->point_offset.element_at(triangle)->offset);
                const IVP_U_Point *point2 = polyhedron->points.element_at(
                    face->point_offset.element_at(triangle + 1)->offset);
                IVP_U_Point edge0;
                IVP_U_Point edge1;
                IVP_U_Point normal;
                edge0.subtract(point1, point0);
                edge1.subtract(point2, point0);
                normal.inline_calc_cross_product(&edge0, &edge1);
                const IVP_DOUBLE sixVolume =
                    -(normal.dot_product(&kernel) - normal.dot_product(point0));
                if (sixVolume <= tolerance)
                    continue;
                auto *subpart = new IVP_Convex_Subpart;
                subpart->points.add(new IVP_U_Point(kernel));
                subpart->points.add(new IVP_U_Point(*point0));
                subpart->points.add(new IVP_U_Point(*point1));
                subpart->points.add(new IVP_U_Point(*point2));
                subparts->add(subpart);
                ++created;
            }
        }
        return created;
    }
};

struct IVP_Template_Surbuild_LedgeSoup {
    IVP_BOOL build_root_convex_hull = IVP_FALSE;
    IVP_BOOL free_input_compact_ledges = IVP_TRUE;
    IVP_BOOL link_to_input_compact_ledges = IVP_FALSE;
    IVP_SURBUILD_LEDGE_SOUP_MERGE_POINT_TYPES merge_points =
        IVP_SLMP_MERGE_AND_REALLOCATE;
};

class alignas(8) IVP_SurfaceBuilder_Ledge_Soup {
    friend struct BML_IvpSurfaceBuilderLedgeSoupLayoutCheck;

public:
    IVP_SurfaceBuilder_Ledge_Soup() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::SurfaceBuilderLedgeSoupConstruct, this,
            [this] {
                compact_surface = nullptr;
                number_of_terminal_spheres = 0;
                number_of_nodes = 0;
                smallest_radius = 0.0;
                size_of_tree_in_bytes = 0;
                spheres_cluster = nullptr;
                ::new (static_cast<void *>(&c_ledge_vec))
                    IVP_U_Vector<IVP_Compact_Ledge>();
                ::new (static_cast<void *>(&rec_spheres))
                    IVP_U_Vector<IVV_Sphere>();
                ::new (static_cast<void *>(&terminal_spheres))
                    IVP_U_Vector<IVV_Sphere>();
                extents_min.k[0] = 1000000.0f;
                extents_min.k[1] = 1000000.0f;
                extents_min.k[2] = 1000000.0f;
                extents_min.hesse_val = 0.0f;
                extents_max.k[0] = -1000000.0f;
                extents_max.k[1] = -1000000.0f;
                extents_max.k[2] = -1000000.0f;
                extents_max.hesse_val = 0.0f;
                longest_axis = 0;
                number_of_unclustered_spheres = 0;
                interval_minhash = nullptr;
                ::new (static_cast<void *>(&overlapping_spheres))
                    IVP_U_Vector<IVV_Sphere>();
                ::new (static_cast<void *>(&built_spheres))
                    IVP_U_Vector<IVV_Sphere>();
                parameters = nullptr;
                first_compact_ledge = nullptr;
                first_poly_point = nullptr;
                n_poly_points_allocated = 0;
                point_hash = nullptr;
                ledgetree_work = nullptr;
                clt_highmem = nullptr;
                clt_lowmem = nullptr;
                ::new (static_cast<void *>(&all_spheres))
                    IVP_U_Vector<IVV_Sphere>();
            });
    }
    ~IVP_SurfaceBuilder_Ledge_Soup() {
        BML::IVP::ABI::InvokeThisOr<void>(
            BML::IVP::ABI::Address::SurfaceBuilderLedgeSoupDestruct, this,
            [this] {
                all_spheres.~IVP_U_Vector<IVV_Sphere>();
                built_spheres.~IVP_U_Vector<IVV_Sphere>();
                overlapping_spheres.~IVP_U_Vector<IVV_Sphere>();
                terminal_spheres.~IVP_U_Vector<IVV_Sphere>();
                rec_spheres.~IVP_U_Vector<IVV_Sphere>();
                c_ledge_vec.~IVP_U_Vector<IVP_Compact_Ledge>();
            });
    }

    void insert_ledge(IVP_Compact_Ledge *ledge) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::SurfaceBuilderLedgeSoupInsert,
            this, ledge);
    }
    IVP_Compact_Surface *compile(
        IVP_Template_Surbuild_LedgeSoup *definition = nullptr) {
        return BML::IVP::ABI::InvokeThis<IVP_Compact_Surface *>(
            BML::IVP::ABI::Address::SurfaceBuilderLedgeSoupCompile,
            this, definition);
    }

private:
    IVP_Compact_Surface *compact_surface;
    int number_of_terminal_spheres;
    int number_of_nodes;
    IVP_DOUBLE smallest_radius;
    int size_of_tree_in_bytes;
    IVV_Sphere_Cluster *spheres_cluster;
    union {
        IVP_U_Vector<IVP_Compact_Ledge> c_ledge_vec;
    };
    union {
        IVP_U_Vector<IVV_Sphere> rec_spheres;
    };
    union {
        IVP_U_Vector<IVV_Sphere> terminal_spheres;
    };
    IVP_U_Float_Point extents_min;
    IVP_U_Float_Point extents_max;
    int longest_axis;
    int number_of_unclustered_spheres;
    IVV_Cluster_Min_Hash *interval_minhash;
    union {
        IVP_U_Vector<IVV_Sphere> overlapping_spheres;
    };
    union {
        IVP_U_Vector<IVV_Sphere> built_spheres;
    };
    IVP_Template_Surbuild_LedgeSoup *parameters;
    IVP_Compact_Ledge *first_compact_ledge;
    IVP_Compact_Poly_Point *first_poly_point;
    int n_poly_points_allocated;
    IVP_I_FPoint_VHash *point_hash;
    IVP_Compact_Ledgetree_Node *ledgetree_work;
    char *clt_highmem;
    char *clt_lowmem;

protected:
    union {
        IVP_U_Vector<IVV_Sphere> all_spheres;
    };
};

struct BML_IvpSurfaceBuilderLedgeSoupLayoutCheck {
    static constexpr std::size_t compact_surface =
        offsetof(IVP_SurfaceBuilder_Ledge_Soup, compact_surface);
    static constexpr std::size_t smallest_radius =
        offsetof(IVP_SurfaceBuilder_Ledge_Soup, smallest_radius);
    static constexpr std::size_t ledges =
        offsetof(IVP_SurfaceBuilder_Ledge_Soup, c_ledge_vec);
    static constexpr std::size_t extents_min =
        offsetof(IVP_SurfaceBuilder_Ledge_Soup, extents_min);
    static constexpr std::size_t interval_minhash =
        offsetof(IVP_SurfaceBuilder_Ledge_Soup, interval_minhash);
    static constexpr std::size_t parameters =
        offsetof(IVP_SurfaceBuilder_Ledge_Soup, parameters);
    static constexpr std::size_t ledgetree_work =
        offsetof(IVP_SurfaceBuilder_Ledge_Soup, ledgetree_work);
    static constexpr std::size_t all_spheres =
        offsetof(IVP_SurfaceBuilder_Ledge_Soup, all_spheres);
};

class IVP_SurMan_PS_Plane : public IVP_U_Point {
public:
    IVP_SurMan_PS_Plane() = default;

    IVP_U_Vector<IVP_U_Point> points;

    IVP_DOUBLE get_area_size() {
        if (points.len() == 0)
            return 0.0;
        IVP_DOUBLE sum = 0.0;
        IVP_U_Point *point0 = points.element_at(0);
        for (int index = 0; index < points.len(); ++index) {
            IVP_U_Point difference0;
            IVP_U_Point difference1;
            IVP_U_Point cross;
            difference0.subtract(
                points.element_at((index + 1) % points.len()),
                points.element_at(index));
            difference1.subtract(points.element_at(index), point0);
            cross.calc_cross_product(&difference0, &difference1);
            sum += cross.dot_product(this);
        }
        return sum;
    }

    IVP_DOUBLE get_qlen_of_all_edges() {
        return BML::IVP::ABI::InvokeThisOr<IVP_DOUBLE>(
            BML::IVP::ABI::Address::SurManPsPlaneGetEdgeQuadLength,
            this,
            [this]() {
                IVP_DOUBLE sum = 0.0;
                for (int index = 0; index < points.len(); ++index) {
                    sum += points.element_at(index)->quad_distance_to(
                        points.element_at((index + 1) % points.len()));
                }
                return sum;
            });
    }
};

// Fixed-capacity public helper used to submit common Ballance-size point sets
// without allocating the vector's pointer array.
class IVP_Vector_of_Points_256 : public IVP_U_Vector<IVP_U_Point> {
private:
    IVP_Core *elem_buffer[256];

public:
    IVP_Vector_of_Points_256()
        : IVP_U_Vector<IVP_U_Point>(
              reinterpret_cast<void **>(&elem_buffer[0]), 256) {}
};

class IVP_SurfaceBuilder_Pointsoup {
public:
    static void cleanup() {
        using TriangleBuilder = IVP_Compact_Ledge *(__cdecl *)(
            IVP_U_Point *, IVP_U_Point *, IVP_U_Point *);
        TriangleBuilder builder = BML::IVP::ABI::Resolve<TriangleBuilder>(
            BML::IVP::ABI::Address::SurfaceBuilderPointsoupTriangleToLedge);
        if (!builder)
            return;
        const std::uintptr_t imageBase =
            reinterpret_cast<std::uintptr_t>(builder) -
            static_cast<std::uint32_t>(
                BML::IVP::ABI::Address::SurfaceBuilderPointsoupTriangleToLedge);
        auto **cached = reinterpret_cast<IVP_Compact_Ledge **>(
            imageBase + BML::IVP::ABI::PointsoupSingleTriangleRva);
        if (*cached)
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::FreeAligned, *cached);
        *cached = nullptr;
    }

    static IVP_Compact_Ledge *convert_triangle_to_compace_ledge(
        IVP_U_Point *point0, IVP_U_Point *point1, IVP_U_Point *point2) {
        return BML::IVP::ABI::Invoke<IVP_Compact_Ledge *>(
            BML::IVP::ABI::Address::SurfaceBuilderPointsoupTriangleToLedge,
            point0, point1, point2);
    }

    static IVP_Compact_Ledge *convert_pointsoup_to_compact_ledge(
        IVP_U_Vector<IVP_U_Point> *points) {
        return BML::IVP::ABI::Invoke<IVP_Compact_Ledge *>(
            BML::IVP::ABI::Address::SurfaceBuilderPointsoupToLedge,
            points);
    }

    static IVP_Compact_Surface *convert_pointsoup_to_compact_surface(
        IVP_U_Vector<IVP_U_Point> *points) {
        IVP_Compact_Ledge *ledge =
            convert_pointsoup_to_compact_ledge(points);
        if (!ledge)
            return nullptr;
        IVP_SurfaceBuilder_Ledge_Soup soup;
        soup.insert_ledge(ledge);
        return soup.compile();
    }
};

class IVP_SurfaceBuilder_Polyhedron_Concave {
public:
    static int convert_concave_polyhedron_to_compact_ledges(
        IVP_Concave_Polyhedron *polyhedron,
        IVP_Convex_Decompositor_Parameters *parameters,
        IVP_U_BigVector<IVP_Compact_Ledge> *ledges) {
        if (!polyhedron || !ledges)
            return 0;
        IVP_U_BigVector<IVP_Convex_Subpart> subparts;
        const int subpartCount = IVP_Convex_Decompositor::
            perform_convex_decomposition_on_concave_polyhedron(
                polyhedron, parameters, &subparts);
        for (int index = 0; index < subparts.len(); ++index) {
            IVP_Convex_Subpart *subpart = subparts.element_at(index);
            IVP_Compact_Ledge *ledge =
                IVP_SurfaceBuilder_Pointsoup::
                    convert_pointsoup_to_compact_ledge(&subpart->points);
            if (ledge)
                ledges->add(ledge);
            delete subpart;
        }
        return subpartCount;
    }

    static void convert_concave_face_soup_to_compact_ledges(
        IVP_Concave_Polyhedron *polyhedron,
        IVP_U_BigVector<IVP_Compact_Ledge> *ledges) {
        if (!polyhedron || !ledges)
            return;

        IVP_U_Vector<IVP_U_Point> facePoints;
        for (int faceIndex = 0; faceIndex < polyhedron->faces.len();
             ++faceIndex) {
            IVP_Concave_Polyhedron_Face *face =
                polyhedron->faces.element_at(faceIndex);
            facePoints.remove_all();
            if (!face)
                continue;
            for (int pointIndex = 0; pointIndex < face->point_offset.len();
                 ++pointIndex) {
                const int offset =
                    face->point_offset.element_at(pointIndex)->offset;
                if (offset >= 0 && offset < polyhedron->points.len())
                    facePoints.add(polyhedron->points.element_at(offset));
            }
            if (facePoints.len() < 3)
                continue;
            IVP_Compact_Ledge *ledge =
                IVP_SurfaceBuilder_Pointsoup::
                    convert_pointsoup_to_compact_ledge(&facePoints);
            if (ledge)
                ledges->add(ledge);
        }
    }

    static IVP_Compact_Surface *
    convert_concave_polyhedron_to_single_compact_surface(
        IVP_Concave_Polyhedron *polyhedron,
        IVP_Convex_Decompositor_Parameters *parameters) {
        IVP_U_BigVector<IVP_Compact_Ledge> ledges;
        if (convert_concave_polyhedron_to_compact_ledges(
                polyhedron, parameters, &ledges) <= 0 || ledges.len() == 0)
            return nullptr;
        IVP_SurfaceBuilder_Ledge_Soup soup;
        for (int index = 0; index < ledges.len(); ++index)
            soup.insert_ledge(ledges.element_at(index));
        IVP_Template_Surbuild_LedgeSoup definition;
        definition.build_root_convex_hull = IVP_TRUE;
        definition.merge_points = IVP_SLMP_MERGE_AND_REALLOCATE;
        return soup.compile(&definition);
    }

};

class IVP_Halfspacesoup : public IVP_U_Vector<IVP_U_Hesse> {
public:
    IVP_Halfspacesoup() {}

    explicit IVP_Halfspacesoup(const IVP_Compact_Ledge *ledge) {
        if (!ledge || ledge->get_n_points() <= 0)
            return;

        IVP_U_Point center;
        center.set_to_zero();
        for (int index = 0; index < ledge->get_n_points(); ++index)
            center.add(&ledge->get_point_array()[index]);
        center.mult(1.0 / static_cast<IVP_DOUBLE>(ledge->get_n_points()));

        const IVP_Compact_Triangle *triangle = ledge->get_first_triangle();
        for (int index = 0; index < ledge->get_n_triangles();
             ++index, triangle = triangle->get_next_tri()) {
            const IVP_Compact_Edge *edge0 = triangle->get_first_edge();
            const IVP_Compact_Edge *edge1 = edge0->get_next();
            const IVP_Compact_Edge *edge2 = edge1->get_next();
            IVP_U_Hesse plane;
            plane.calc_hesse(
                edge0->get_start_point(ledge),
                edge1->get_start_point(ledge),
                edge2->get_start_point(ledge));
            // Halfspacesoup normals point into the enclosed volume.  Compact
            // triangle winding differs between old builders, so use the
            // interior centroid to choose the same semantic orientation.
            if (plane.get_dist(&center) < 0.0)
                plane.mult_hesse(-1.0);
            add_halfspace(&plane);
        }
    }

    ~IVP_Halfspacesoup() {
        for (int index = len() - 1; index >= 0; --index)
            delete element_at(index);
        clear();
    }

    void add_halfspace(const IVP_U_Hesse *plane) {
        if (!plane)
            return;
        constexpr IVP_DOUBLE tolerance = 0.0001;
        IVP_U_Vector<IVP_U_Hesse> planesToDelete;
        for (int index = 0; index < len(); ++index) {
            IVP_U_Hesse *oldPlane = element_at(index);
            if (plane->dot_product(oldPlane) > 1.0 - tolerance) {
                if (oldPlane->hesse_val < plane->hesse_val)
                    return;
                planesToDelete.add(oldPlane);
            }
        }

        add(new IVP_U_Hesse(*plane));
        for (int index = planesToDelete.len() - 1; index >= 0; --index) {
            IVP_U_Hesse *oldPlane = planesToDelete.element_at(index);
            remove(oldPlane);
            delete oldPlane;
        }
    }
};

class IVP_SurfaceBuilder_Halfspacesoup {
private:
    static IVP_U_Point *insert_point_into_list(
        IVP_U_Point *point, IVP_U_Vector<IVP_U_Point> *points,
        IVP_DOUBLE squaredThreshold) {
        for (int index = 0; index < points->len(); ++index) {
            IVP_U_Point *oldPoint = points->element_at(index);
            if (point->quad_distance_to(oldPoint) < squaredThreshold) {
                delete point;
                return oldPoint;
            }
        }
        points->add(point);
        return point;
    }

public:
    static int convert_halfspacesoup_to_points(
        IVP_Halfspacesoup *halfspaces, IVP_DOUBLE pointmergeThreshold,
        IVP_U_Vector<IVP_U_Point> *points) {
        if (!halfspaces || !points)
            return 0;
        constexpr IVP_DOUBLE tolerance = 0.0001;
        const IVP_DOUBLE squaredThreshold =
            pointmergeThreshold * pointmergeThreshold;

        for (int first = 0; first < halfspaces->len(); ++first) {
            for (int second = first + 1; second < halfspaces->len(); ++second) {
                for (int third = second + 1; third < halfspaces->len(); ++third) {
                    IVP_U_Point crossing;
                    if (crossing.set_crossing(
                            halfspaces->element_at(first),
                            halfspaces->element_at(second),
                            halfspaces->element_at(third)) == IVP_FAULT)
                        continue;

                    bool outside = false;
                    for (int plane = 0; plane < halfspaces->len(); ++plane) {
                        if (halfspaces->element_at(plane)->get_dist(&crossing) <
                            -tolerance) {
                            outside = true;
                            break;
                        }
                    }
                    if (!outside) {
                        insert_point_into_list(
                            new IVP_U_Point(crossing), points,
                            squaredThreshold);
                    }
                }
            }
        }

        for (int first = 0; first < points->len(); ++first) {
            IVP_U_Point *point = points->element_at(first);
            for (int second = points->len() - 1; second > first; --second) {
                IVP_U_Point *candidate = points->element_at(second);
                if (point->quad_distance_to(candidate) < squaredThreshold) {
                    points->remove(candidate);
                    delete candidate;
                }
            }
        }
        return points->len();
    }

    static IVP_Compact_Ledge *convert_halfspacesoup_to_compact_ledge(
        IVP_Halfspacesoup *halfspaces, IVP_DOUBLE pointmergeThreshold) {
        IVP_U_Vector<IVP_U_Point> points;
        convert_halfspacesoup_to_points(
            halfspaces, pointmergeThreshold, &points);
        IVP_Compact_Ledge *ledge =
            IVP_SurfaceBuilder_Pointsoup::convert_pointsoup_to_compact_ledge(
                &points);
        for (int index = points.len() - 1; index >= 0; --index)
            delete points.element_at(index);
        return ledge;
    }

    static IVP_Compact_Surface *convert_halfspacesoup_to_compact_surface(
        IVP_Halfspacesoup *halfspaces, IVP_DOUBLE pointmergeThreshold) {
        IVP_Compact_Ledge *ledge = convert_halfspacesoup_to_compact_ledge(
            halfspaces, pointmergeThreshold);
        if (!ledge)
            return nullptr;
        IVP_SurfaceBuilder_Ledge_Soup soup;
        soup.insert_ledge(ledge);
        return soup.compile();
    }
};

class IVP_SurfaceBuilder_Q12 {
    friend struct BML_IvpSurfaceBuilderQ12LayoutCheck;

private:
    dheader_t *header = nullptr;
    int n_models = 0;
    dmodel_t *dmodels = nullptr;
    int n_planes = 0;
    dplane_t *dplanes = nullptr;
    int n_nodes = 0;
    dnode_t *dnodes = nullptr;
    int n_clipnodes = 0;
    dclipnode_t *dclipnodes = nullptr;

    IVP_q12_int *zero = nullptr;
    IVP_q12_int *one = nullptr;
    IVP_BOOL bsptree_loaded_from_disk = IVP_FALSE;

    IVP_FLOAT min_x = 0.0f;
    IVP_FLOAT min_y = 0.0f;
    IVP_FLOAT min_z = 0.0f;
    IVP_FLOAT max_x = 0.0f;
    IVP_FLOAT max_y = 0.0f;
    IVP_FLOAT max_z = 0.0f;
    IVP_FLOAT shrink_value = 0.0f;
    IVP_FLOAT scale = 1.0f;
    IVP_FLOAT pointmerge_threshold = 0.0f;

    int n_solid_nodes = 0;
    int n_converted_nodes = 0;
    IVP_U_Vector<IVP_Compact_Ledge> *ledges = nullptr;
    IVP_U_Vector<intp> nodes;
    IVP_Halfspacesoup *halfspaces = nullptr;

    template <typename T>
    static bool CopyLump(const std::vector<std::byte> &bytes,
                         const lump_t &lump, T *&destination,
                         int &count) {
        destination = nullptr;
        count = 0;
        if (lump.fileofs < 0 || lump.filelen < 0 ||
            lump.filelen % static_cast<int>(sizeof(T)) != 0)
            return false;
        const std::size_t offset = static_cast<std::size_t>(lump.fileofs);
        const std::size_t length = static_cast<std::size_t>(lump.filelen);
        if (offset > bytes.size() || length > bytes.size() - offset)
            return false;
        count = static_cast<int>(length / sizeof(T));
        if (count == 0)
            return true;
        destination = new (std::nothrow) T[static_cast<std::size_t>(count)];
        if (!destination)
            return false;
        std::memcpy(destination, bytes.data() + offset, length);
        return true;
    }

    void clear_halfspaces() {
        delete halfspaces;
        halfspaces = new (std::nothrow) IVP_Halfspacesoup;
    }

    void create_and_insert_plane(IVP_FLOAT nx, IVP_FLOAT ny,
                                 IVP_FLOAT nz, IVP_FLOAT dist) {
        if (!halfspaces)
            return;
        IVP_U_Hesse plane;
        // Quake uses X/Y/Z; the public IVP adapter maps this to X/-Z/Y.
        plane.set(nx, -nz, ny);
        plane.hesse_val = -(dist + shrink_value) * scale;
        halfspaces->add_halfspace(&plane);
    }

    bool append_path_planes() {
        for (int index = 0; index < nodes.len(); ++index) {
            const int nodeIndex =
                static_cast<int>(reinterpret_cast<intp>(
                    nodes.element_at(index)));
            if (nodeIndex < 0 || nodeIndex >= n_nodes)
                return false;
            const dnode_t &node = dnodes[nodeIndex];
            if (node.planenum < 0 || node.planenum >= n_planes)
                return false;
            const dplane_t &plane = dplanes[node.planenum];
            bool frontSide = false;
            if (index + 1 == nodes.len()) {
                // Preserve the neighboring adapter's final-node convention.
                frontSide = node.children[0] == -1;
            } else {
                const int nextNode =
                    static_cast<int>(reinterpret_cast<intp>(
                        nodes.element_at(index + 1)));
                frontSide = node.children[0] == nextNode;
            }
            const IVP_FLOAT direction = frontSide ? 1.0f : -1.0f;
            create_and_insert_plane(
                direction * plane.normal[0],
                direction * plane.normal[1],
                direction * plane.normal[2], direction * plane.dist);
        }
        return true;
    }

    void convert_solid_node() {
        ++n_solid_nodes;
        if (!append_path_planes()) {
            clear_halfspaces();
            return;
        }
        create_and_insert_plane(1.0f, 0.0f, 0.0f, min_x);
        create_and_insert_plane(0.0f, 1.0f, 0.0f, min_y);
        create_and_insert_plane(0.0f, 0.0f, 1.0f, min_z);
        create_and_insert_plane(-1.0f, 0.0f, 0.0f, -max_x);
        create_and_insert_plane(0.0f, -1.0f, 0.0f, -max_y);
        create_and_insert_plane(0.0f, 0.0f, -1.0f, -max_z);
        IVP_Compact_Ledge *ledge =
            IVP_SurfaceBuilder_Halfspacesoup::
                convert_halfspacesoup_to_compact_ledge(
                    halfspaces, pointmerge_threshold);
        if (ledge && ledges) {
            ledges->add(ledge);
            ++n_converted_nodes;
        }
        clear_halfspaces();
    }

    void convert_node(intp nodeIndex, int depth) {
        if (nodeIndex < 0 || nodeIndex >= n_nodes || depth > n_nodes)
            return;
        for (int index = 0; index < nodes.len(); ++index) {
            if (reinterpret_cast<intp>(nodes.element_at(index)) == nodeIndex)
                return;
        }
        nodes.add(reinterpret_cast<intp *>(nodeIndex));
        const dnode_t &node = dnodes[nodeIndex];
        for (int side = 0; side < 2; ++side) {
            if (node.children[side] >= 0)
                convert_node(node.children[side], depth + 1);
            else if (node.children[side] == -1)
                convert_solid_node();
        }
        nodes.remove_at(nodes.len() - 1);
    }

    void convert_model(int model) {
        const IVP_FLOAT expansion = 1.0f / scale;
        min_x = dmodels[model].mins[0] - expansion;
        min_y = dmodels[model].mins[1] - expansion;
        min_z = dmodels[model].mins[2] - expansion;
        max_x = dmodels[model].maxs[0] + expansion;
        max_y = dmodels[model].maxs[1] + expansion;
        max_z = dmodels[model].maxs[2] + expansion;
        n_solid_nodes = 0;
        n_converted_nodes = 0;
        nodes.remove_all();
        convert_node(dmodels[model].headnode[0], 0);
        nodes.remove_all();
    }

public:
    IVP_SurfaceBuilder_Q12()
        : zero(new IVP_q12_int(0)), one(new IVP_q12_int(1)),
          halfspaces(new IVP_Halfspacesoup) {}

    ~IVP_SurfaceBuilder_Q12() {
        unload_q12bsp();
        delete zero;
        delete one;
        delete halfspaces;
    }

    int load_q12bsp_file(char *filename) {
        if (!filename)
            return 0;
        std::ifstream input(filename, std::ios::binary | std::ios::ate);
        if (!input)
            return 0;
        const std::streamoff fileLength = input.tellg();
        if (fileLength < static_cast<std::streamoff>(sizeof(dheader_t)) ||
            static_cast<std::uintmax_t>(fileLength) >
                std::numeric_limits<std::size_t>::max())
            return 0;
        std::vector<std::byte> bytes(static_cast<std::size_t>(fileLength));
        input.seekg(0);
        if (!input.read(reinterpret_cast<char *>(bytes.data()), fileLength))
            return 0;
        dheader_t fileHeader{};
        std::memcpy(&fileHeader, bytes.data(), sizeof(fileHeader));
        constexpr int kBspVersion = 30;
        if (fileHeader.version < 0 || fileHeader.version > kBspVersion)
            return 0;

        unload_q12bsp();
        dmodel_t *models = nullptr;
        dplane_t *planes = nullptr;
        dnode_t *fileNodes = nullptr;
        dclipnode_t *clipnodes = nullptr;
        int modelCount = 0;
        int planeCount = 0;
        int nodeCount = 0;
        int clipnodeCount = 0;
        constexpr int kPlanesLump = 1;
        constexpr int kNodesLump = 5;
        constexpr int kClipnodesLump = 9;
        constexpr int kModelsLump = 14;
        if (!CopyLump(bytes, fileHeader.lumps[kModelsLump], models,
                      modelCount) ||
            !CopyLump(bytes, fileHeader.lumps[kPlanesLump], planes,
                      planeCount) ||
            !CopyLump(bytes, fileHeader.lumps[kNodesLump], fileNodes,
                      nodeCount) ||
            !CopyLump(bytes, fileHeader.lumps[kClipnodesLump], clipnodes,
                      clipnodeCount)) {
            delete[] models;
            delete[] planes;
            delete[] fileNodes;
            delete[] clipnodes;
            return 0;
        }
        dmodels = models;
        dplanes = planes;
        dnodes = fileNodes;
        dclipnodes = clipnodes;
        n_models = modelCount;
        n_planes = planeCount;
        n_nodes = nodeCount;
        n_clipnodes = clipnodeCount;
        bsptree_loaded_from_disk = IVP_TRUE;
        return n_models;
    }

    void init_q12bsp_from_memory(
        int version, int n_models_in, dmodel_t *dmodels_in,
        int n_planes_in, dplane_t *dplanes_in, int n_nodes_in,
        dnode_t *dnodes_in, int n_clipnodes_in,
        dclipnode_t *dclipnodes_in) {
        unload_q12bsp();
        if (version < 0 || version > 30 || n_models_in < 0 ||
            n_planes_in < 0 || n_nodes_in < 0 || n_clipnodes_in < 0)
            return;
        n_models = n_models_in;
        dmodels = dmodels_in;
        n_planes = n_planes_in;
        dplanes = dplanes_in;
        n_nodes = n_nodes_in;
        dnodes = dnodes_in;
        n_clipnodes = n_clipnodes_in;
        dclipnodes = dclipnodes_in;
        bsptree_loaded_from_disk = IVP_FALSE;
    }

    void unload_q12bsp() {
        if (bsptree_loaded_from_disk) {
            delete[] dmodels;
            delete[] dplanes;
            delete[] dnodes;
            delete[] dclipnodes;
        }
        header = nullptr;
        n_models = n_planes = n_nodes = n_clipnodes = 0;
        dmodels = nullptr;
        dplanes = nullptr;
        dnodes = nullptr;
        dclipnodes = nullptr;
        bsptree_loaded_from_disk = IVP_FALSE;
    }

    void convert_q12bsp_model_to_compact_ledges(
        int model, IVP_DOUBLE scaling_factor,
        IVP_DOUBLE shrink_value_in, IVP_FLOAT pointmerge_threshold_in,
        IVP_U_Vector<IVP_Compact_Ledge> *ledges_out) {
        if (!dmodels || !dplanes || !dnodes || !ledges_out ||
            model < 0 || model >= n_models ||
            !std::isfinite(scaling_factor) || scaling_factor <= 0.0 ||
            !std::isfinite(shrink_value_in) ||
            !std::isfinite(pointmerge_threshold_in) ||
            pointmerge_threshold_in < 0.0f)
            return;
        scale = static_cast<IVP_FLOAT>(scaling_factor);
        shrink_value = static_cast<IVP_FLOAT>(
            shrink_value_in / scaling_factor);
        pointmerge_threshold = pointmerge_threshold_in;
        ledges = ledges_out;
        convert_model(model);
        ledges = nullptr;
    }

    IVP_Compact_Surface *convert_q12bsp_model_to_single_compact_surface(
        int model, IVP_DOUBLE scaling_factor,
        IVP_DOUBLE shrink_value_in, IVP_FLOAT pointmerge_threshold_in) {
        IVP_U_Vector<IVP_Compact_Ledge> localLedges;
        convert_q12bsp_model_to_compact_ledges(
            model, scaling_factor, shrink_value_in,
            pointmerge_threshold_in, &localLedges);
        if (localLedges.len() == 0)
            return nullptr;
        IVP_SurfaceBuilder_Ledge_Soup soup;
        for (int index = 0; index < localLedges.len(); ++index)
            soup.insert_ledge(localLedges.element_at(index));
        return soup.compile();
    }
};

class IVP_Compact_Modify {
public:
    static IVP_Compact_Surface *chop(
        const IVP_Compact_Surface *surface,
        const IVP_U_Float_Point *chopVector, IVP_FLOAT chopDepth) {
        if (!surface || !chopVector)
            return nullptr;
        IVP_U_BigVector<IVP_Compact_Ledge> ledges;
        IVP_SurfaceManager_Polygon manager(surface);
        manager.get_all_terminal_ledges(&ledges);
        if (ledges.len() != 1)
            return nullptr;

        IVP_U_Point direction;
        direction.set(chopVector);
        if (direction.normize() == IVP_FAULT)
            return nullptr;

        const IVP_Compact_Ledge *ledge = ledges.element_at(0);
        IVP_DOUBLE minimumProjection = 0.0;
        bool havePoint = false;
        for (int index = 0; index < ledge->get_n_points(); ++index) {
            const IVP_DOUBLE projection =
                direction.dot_product(&ledge->get_point_array()[index]);
            if (!havePoint || projection < minimumProjection) {
                minimumProjection = projection;
                havePoint = true;
            }
        }
        if (!havePoint)
            return nullptr;

        IVP_Halfspacesoup halfspaces(ledge);
        IVP_U_Hesse chopPlane;
        chopPlane.set(&direction);
        IVP_U_Point chopPoint;
        chopPoint.set(&direction);
        chopPoint.mult(minimumProjection + chopDepth);
        chopPlane.calc_hesse_val(&chopPoint);
        halfspaces.add_halfspace(&chopPlane);
        return IVP_SurfaceBuilder_Halfspacesoup::
            convert_halfspacesoup_to_compact_surface(
                &halfspaces, 0.01);
    }

    static IVP_Compact_Ledge *shrink(
        const IVP_Compact_Ledge *ledge, IVP_FLOAT shrinkValue,
        IVP_DOUBLE pointmergeThreshold) {
        if (!ledge)
            return nullptr;
        IVP_Halfspacesoup halfspaces(ledge);
        for (int index = halfspaces.len() - 1; index >= 0; --index)
            halfspaces.element_at(index)->hesse_val -= shrinkValue;
        return IVP_SurfaceBuilder_Halfspacesoup::
            convert_halfspacesoup_to_compact_ledge(
                &halfspaces, pointmergeThreshold);
    }

    static IVP_Compact_Surface *shrink(
        const IVP_Compact_Surface *surface, IVP_FLOAT shrinkValue,
        IVP_DOUBLE pointmergeThreshold) {
        if (!surface)
            return nullptr;
        IVP_U_BigVector<IVP_Compact_Ledge> ledges;
        IVP_SurfaceManager_Polygon manager(surface);
        manager.get_all_terminal_ledges(&ledges);

        IVP_SurfaceBuilder_Ledge_Soup soup;
        int inserted = 0;
        for (int index = ledges.len() - 1; index >= 0; --index) {
            IVP_Compact_Ledge *shrunken = shrink(
                ledges.element_at(index), shrinkValue,
                pointmergeThreshold);
            if (shrunken) {
                soup.insert_ledge(shrunken);
                ++inserted;
            }
        }
        return inserted > 0 ? soup.compile() : nullptr;
    }
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Template_Surbuild_LedgeSoup) == 0x10);
static_assert(sizeof(IVP_Concave_Polyhedron_Face_Pointoffset) == 0x04);
static_assert(sizeof(IVP_Concave_Polyhedron_Face) == 0x08);
static_assert(offsetof(IVP_Concave_Polyhedron_Face, point_offset) == 0x00);
static_assert(sizeof(IVP_Concave_Polyhedron) == 0x18);
static_assert(sizeof(IVP_Template_SurfaceBuilder_3ds) == 0x04);
static_assert(offsetof(IVP_Template_SurfaceBuilder_3ds, scale) == 0x00);
static_assert(sizeof(IVP_SurfaceBuilder_3ds) == 0x01);
static_assert(sizeof(dmodel_t) == 0x40);
static_assert(sizeof(lump_t) == 0x08);
static_assert(sizeof(dheader_t) == 0x7C);
static_assert(sizeof(dplane_t) == 0x14);
static_assert(sizeof(dnode_t) == 0x18);
static_assert(sizeof(dclipnode_t) == 0x08);
static_assert(sizeof(IVP_q12_int) == 0x04);
static_assert(sizeof(IVP_Convex_Subpart) == 0x08);
static_assert(offsetof(IVP_Convex_Subpart, points) == 0x00);
static_assert(sizeof(IVP_Convex_Decompositor_Parameters) == 0x0C);
static_assert(sizeof(IVP_SurfaceBuilder_Polyhedron_Concave) == 0x01);
static_assert(sizeof(IVP_Convex_Decompositor) == 0x01);
static_assert(sizeof(IVP_SurfaceBuilder_Ledge_Soup) == 0xA0);
static_assert(alignof(IVP_SurfaceBuilder_Ledge_Soup) == 0x08);
static_assert(BML_IvpSurfaceBuilderLedgeSoupLayoutCheck::compact_surface == 0x00);
static_assert(BML_IvpSurfaceBuilderLedgeSoupLayoutCheck::smallest_radius == 0x10);
static_assert(BML_IvpSurfaceBuilderLedgeSoupLayoutCheck::ledges == 0x20);
static_assert(BML_IvpSurfaceBuilderLedgeSoupLayoutCheck::extents_min == 0x38);
static_assert(BML_IvpSurfaceBuilderLedgeSoupLayoutCheck::interval_minhash == 0x60);
static_assert(BML_IvpSurfaceBuilderLedgeSoupLayoutCheck::parameters == 0x74);
static_assert(BML_IvpSurfaceBuilderLedgeSoupLayoutCheck::ledgetree_work == 0x88);
static_assert(BML_IvpSurfaceBuilderLedgeSoupLayoutCheck::all_spheres == 0x94);
static_assert(sizeof(IVP_SurMan_PS_Plane) == 0x28);
static_assert(offsetof(IVP_SurMan_PS_Plane, points) == 0x20);
static_assert(sizeof(IVP_Vector_of_Points_256) == 0x408);
static_assert(sizeof(IVP_Halfspacesoup) == 0x08);
static_assert(sizeof(IVP_SurfaceBuilder_Halfspacesoup) == 0x01);
static_assert(sizeof(IVP_SurfaceBuilder_Q12) == 0x6C);
static_assert(sizeof(IVP_Compact_Modify) == 0x01);

struct BML_IvpSurfaceBuilderQ12LayoutCheck {
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, header) == 0x00);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, dmodels) == 0x08);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, dplanes) == 0x10);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, dnodes) == 0x18);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, dclipnodes) == 0x20);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, zero) == 0x24);
    static_assert(
        offsetof(IVP_SurfaceBuilder_Q12, bsptree_loaded_from_disk) == 0x2C);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, min_x) == 0x30);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, max_z) == 0x44);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, shrink_value) == 0x48);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, scale) == 0x4C);
    static_assert(
        offsetof(IVP_SurfaceBuilder_Q12, pointmerge_threshold) == 0x50);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, n_solid_nodes) == 0x54);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, ledges) == 0x5C);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, nodes) == 0x60);
    static_assert(offsetof(IVP_SurfaceBuilder_Q12, halfspaces) == 0x68);
};
#endif

#endif // BML_IVP_SURFACE_BUILDER_H
