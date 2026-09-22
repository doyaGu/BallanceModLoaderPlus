#ifndef BML_IVP_RAY_H
#define BML_IVP_RAY_H

#include "BML/IVP/Broadphase.h"
#include "BML/IVP/Cache.h"
#include "BML/IVP/MinHash.h"
#include "BML/IVP/Surface.h"

#include <cstddef>

#ifndef IVP_MAX_NUM_RAY_HITS
#define IVP_MAX_NUM_RAY_HITS 256
#endif

class IVP_SurfaceManager_Grid;

enum IVP_RAY_SOLVER_FLAGS : std::int32_t {
    IVP_RAY_SOLVER_ALL = 0,
    IVP_RAY_SOLVER_IGNORE_PHANTOMS = 1,
    IVP_RAY_SOLVER_IGNORE_MOVINGS = 2,
    IVP_RAY_SOLVER_IGNORE_STATICS = 4,
};

struct IVP_Ray_Hit {
    IVP_U_Float_Point hit_surface_direction_os;
    IVP_Real_Object *hit_real_object = nullptr;
    const IVP_Compact_Ledge *hit_compact_ledge = nullptr;
    const IVP_Compact_Triangle *hit_compact_triangle = nullptr;
    IVP_FLOAT hit_distance = 0.0f;
};

struct IVP_Ray_Solver_Template {
    IVP_U_Point ray_start_point;
    IVP_U_Float_Point ray_normized_direction;
    IVP_FLOAT ray_length = 0.0f;
    IVP_RAY_SOLVER_FLAGS ray_flags = IVP_RAY_SOLVER_ALL;
};

class IVP_Ray_Solver_Os;

// The original listener has exactly two slots: hit callback and destructor.
class IVP_Ray_Hit_Listener {
public:
    virtual void add_hit_object(
        IVP_Real_Object *object, const IVP_Compact_Ledge *ledge,
        const IVP_Compact_Triangle *triangle, IVP_DOUBLE hitDistance,
        IVP_U_Point *surfaceDirectionObject) = 0;
    virtual ~IVP_Ray_Hit_Listener() = default;
};

class IVP_Ray_Solver : public IVP_Ray_Hit_Listener {
public:
    explicit IVP_Ray_Solver(const IVP_Ray_Solver_Template *configuration) {
        if (!configuration)
            return;
        ray_direction.set(&configuration->ray_normized_direction);
        ray_start_point.set(&configuration->ray_start_point);
        ray_length = configuration->ray_length;
        ray_flags = configuration->ray_flags;
        ray_center_point.set(&ray_start_point);
        ray_center_point.add_multiple(&ray_direction, ray_length * 0.5f);
        ray_end_point.add_multiple(&ray_start_point, &ray_direction, ray_length);
    }
    ~IVP_Ray_Solver() override = default;

    IVP_BOOL check_ray_against_sphere(
        const IVP_U_Float_Point *sphereCenterWs,
        IVP_FLOAT sphereRadius) {
        IVP_U_Float_Point centerDelta;
        centerDelta.subtract(sphereCenterWs, &ray_center_point);
        const IVP_DOUBLE broadRadius =
            ray_length * 0.5f + sphereRadius;
        if (centerDelta.quad_length() >= broadRadius * broadRadius)
            return IVP_FALSE;

        IVP_U_Float_Point perpendicular;
        perpendicular.inline_calc_cross_product(
            &ray_direction, &centerDelta);
        return perpendicular.quad_length() < sphereRadius * sphereRadius
            ? IVP_TRUE
            : IVP_FALSE;
    }

    IVP_BOOL check_ray_against_square(
        IVP_FLOAT positionDistance, IVP_FLOAT positionAxisLength,
        const IVP_U_Float_Point *minimum,
        const IVP_U_Float_Point *maximum,
        int coordinate0, int coordinate1) {
        const int coordinates[2] = {coordinate0, coordinate1};
        for (int index = 1; index >= 0; --index) {
            const int coordinate = coordinates[index];
            const IVP_FLOAT projected = positionDistance * ray_length *
                                        ray_direction.k[coordinate];
            if (projected <
                    (minimum->k[coordinate] -
                     static_cast<IVP_FLOAT>(ray_start_point.k[coordinate])) *
                        positionAxisLength ||
                projected >
                    (maximum->k[coordinate] -
                     static_cast<IVP_FLOAT>(ray_start_point.k[coordinate])) *
                        positionAxisLength) {
                return IVP_FALSE;
            }
        }
        return IVP_TRUE;
    }

    IVP_BOOL check_ray_against_cube(
        const IVP_U_Float_Point *minimum,
        const IVP_U_Float_Point *maximum) {
        IVP_DOUBLE entry = 0.0;
        IVP_DOUBLE exit = ray_length;
        for (int axis = 0; axis < 3; ++axis) {
            const IVP_DOUBLE start = ray_start_point.k[axis];
            const IVP_DOUBLE direction = ray_direction.k[axis];
            if (std::fabs(direction) <= 1.0e-12) {
                if (start < minimum->k[axis] || start > maximum->k[axis])
                    return IVP_FALSE;
                continue;
            }

            IVP_DOUBLE nearDistance =
                (minimum->k[axis] - start) / direction;
            IVP_DOUBLE farDistance =
                (maximum->k[axis] - start) / direction;
            if (nearDistance > farDistance)
                std::swap(nearDistance, farDistance);
            entry = std::max(entry, nearDistance);
            exit = std::min(exit, farDistance);
            if (entry > exit)
                return IVP_FALSE;
        }
        return IVP_TRUE;
    }

    // These methods are inline in the IVP revision embedded by Ballance, so
    // there is no retail function address to call. The reconstruction keeps
    // the retail object-type dispatch and analytical sphere intersection;
    // polygon traversal remains delegated to the original surface-manager
    // vtable.
    void check_ray_against_ball(IVP_Ball *ball) {
        if (!ball)
            return;

        IVP_Cache_Object *cache = ball->get_cache_object_no_lock();
        if (!cache)
            return;

        IVP_U_Float_Point centerWorld;
        centerWorld.set(cache->m_world_f_object.get_position());
        const IVP_DOUBLE radius = ball->get_radius();
        const IVP_DOUBLE radiusSquared = radius * radius;
        const IVP_DOUBLE startDistanceSquared =
            ray_start_point.quad_distance_to(&centerWorld);
        if (startDistanceSquared < radiusSquared)
            return;

        IVP_U_Point equation;
        IVP_U_Point delta;
        delta.set(ray_start_point.k[0] - centerWorld.k[0],
                  ray_start_point.k[1] - centerWorld.k[1],
                  ray_start_point.k[2] - centerWorld.k[2]);
        equation.k[0] = 1.0;
        equation.k[1] = 2.0 * delta.dot_product(&ray_direction);
        equation.k[2] = startDistanceSquared - radiusSquared;

        IVP_U_Point solution;
        solution.solve_quadratic_equation_accurate(&equation);
        if (solution.k[0] < 0.0)
            return;
        IVP_DOUBLE hitDistance = solution.k[1];
        if (hitDistance < 0.0) {
            hitDistance = solution.k[2];
            if (hitDistance < 0.0)
                return;
        }
        IVP_U_Point hitPoint;
        hitPoint.add_multiple(&ray_start_point, &ray_direction, hitDistance);
        IVP_U_Point surfaceNormal;
        surfaceNormal.set(hitPoint.k[0] - centerWorld.k[0],
                          hitPoint.k[1] - centerWorld.k[1],
                          hitPoint.k[2] - centerWorld.k[2]);
        if (surfaceNormal.normize() != IVP_OK)
            surfaceNormal.set(1.0, 0.0, 0.0);
        add_hit_object(ball, nullptr, nullptr, hitDistance, &surfaceNormal);
    }

    void check_ray_against_object(IVP_Real_Object *object) {
        if (!object)
            return;
        if ((ray_flags & IVP_RAY_SOLVER_IGNORE_PHANTOMS) != 0 &&
            object->get_controller_phantom())
            return;

        const auto movement =
            static_cast<std::uint32_t>(object->get_movement_state());
        const bool isStatic = (movement & 0x10u) != 0u;
        if ((ray_flags & IVP_RAY_SOLVER_IGNORE_MOVINGS) != 0 && !isStatic)
            return;
        if ((ray_flags & IVP_RAY_SOLVER_IGNORE_STATICS) != 0 && isStatic)
            return;

        switch (object->get_type()) {
        case IVP_BALL:
            check_ray_against_ball(object->to_ball());
            break;
        case IVP_POLYGON:
            if (IVP_SurfaceManager *surface = object->get_surface_manager())
                surface->insert_all_ledges_hitting_ray(this, object);
            break;
        default:
            break;
        }
    }

    void check_ray_against_compact_ledge_os(
        const IVP_Compact_Ledge *ledge, IVP_Real_Object *object);

    void check_ray_against_node(
        IVP_OV_Node *node, IVP_OV_Tree_Manager *treeManager) {
        if (!node || !treeManager)
            return;

        IVP_U_Float_Point minimum;
        IVP_FLOAT cubeSize = 0.0f;
        treeManager->get_luf_coordinates_ws(node, &minimum, &cubeSize);
        IVP_U_Float_Point maximum(
            minimum.k[0] + cubeSize,
            minimum.k[1] + cubeSize,
            minimum.k[2] + cubeSize);
        if (check_ray_against_cube(&minimum, &maximum) == IVP_FALSE)
            return;

        for (int index = node->elements.len() - 1; index >= 0; --index) {
            IVP_OV_Element *element = node->elements.element_at(index);
            IVP_Real_Object *object = element ? element->real_object : nullptr;
            IVP_Core *core = object ? object->get_core() : nullptr;
            if (!core)
                continue;
            IVP_U_Float_Point center(core->get_position_PSI());
            if (check_ray_against_sphere(
                    &center, core->upper_limit_radius) != IVP_FALSE) {
                check_ray_against_object(object);
            }
        }

        for (int index = node->children.len() - 1; index >= 0; --index)
            check_ray_against_node(
                node->children.element_at(index), treeManager);
    }

    void check_ray_against_all_objects_in_sim(
        const IVP_Environment *environment) {
        IVP_OV_Tree_Manager *treeManager =
            environment ? environment->get_ov_tree_manager() : nullptr;
        if (treeManager)
            check_ray_against_node(treeManager->root, treeManager);
    }

    IVP_U_Point ray_start_point;
    IVP_U_Point ray_end_point;
    IVP_U_Float_Point ray_center_point;
    IVP_U_Float_Point ray_direction;
    IVP_FLOAT ray_length = 0.0f;
    IVP_RAY_SOLVER_FLAGS ray_flags = IVP_RAY_SOLVER_ALL;
};

class IVP_Ray_Solver_Group {
public:
    IVP_Ray_Solver_Group(
        int raySolverCount, IVP_Ray_Solver **raySolverArray)
        : radius(0.0f), n_ray_solvers(raySolverCount),
          ray_solvers(raySolverArray) {
        center_ws.set_to_zero();
        if (n_ray_solvers <= 0 || !ray_solvers)
            return;
        for (int index = 0; index < n_ray_solvers; ++index)
            center_ws.add(&ray_solvers[index]->ray_center_point);
        center_ws.mult(1.0 / n_ray_solvers);

        IVP_DOUBLE radiusSquared = 0.0;
        for (int index = 0; index < n_ray_solvers; ++index) {
            radiusSquared = std::max(
                radiusSquared,
                ray_solvers[index]->ray_start_point.quad_distance_to(
                    &center_ws));
            radiusSquared = std::max(
                radiusSquared,
                ray_solvers[index]->ray_end_point.quad_distance_to(
                    &center_ws));
        }
        radius = static_cast<IVP_FLOAT>(std::sqrt(radiusSquared));
    }

    IVP_BOOL check_ray_group_against_cube(
        const IVP_U_Float_Point *cubeCenterWs, IVP_FLOAT cubeSize) {
        IVP_U_Float_Point distance;
        distance.subtract(cubeCenterWs, &center_ws);
        distance.set(std::fabs(distance.k[0]), std::fabs(distance.k[1]),
                     std::fabs(distance.k[2]));
        const IVP_FLOAT broadRadius = cubeSize * 0.5f + radius;
        if (distance.k[0] > broadRadius ||
            distance.k[1] > broadRadius ||
            distance.k[2] > broadRadius) {
            return IVP_FALSE;
        }
        return distance.quad_length() <
                       broadRadius * broadRadius * 3.0f
            ? IVP_TRUE
            : IVP_FALSE;
    }

    void check_ray_group_against_object(IVP_Real_Object *object) {
        if (!object || n_ray_solvers <= 0 || !ray_solvers)
            return;
        const IVP_RAY_SOLVER_FLAGS flags = ray_solvers[0]->ray_flags;
        if ((flags & IVP_RAY_SOLVER_IGNORE_PHANTOMS) != 0 &&
            object->get_controller_phantom()) {
            return;
        }
        const auto movement =
            static_cast<std::uint32_t>(object->get_movement_state());
        const bool isStatic = (movement & 0x10u) != 0u;
        if ((flags & IVP_RAY_SOLVER_IGNORE_MOVINGS) != 0 && !isStatic)
            return;
        if ((flags & IVP_RAY_SOLVER_IGNORE_STATICS) != 0 && isStatic)
            return;

        if (object->get_type() == IVP_BALL) {
            IVP_Ball *ball = object->to_ball();
            for (int index = n_ray_solvers - 1; index >= 0; --index)
                ray_solvers[index]->check_ray_against_ball(ball);
        } else if (object->get_type() == IVP_POLYGON) {
            IVP_SurfaceManager *surface = object->get_surface_manager();
            if (!surface)
                return;
            for (int index = n_ray_solvers - 1; index >= 0; --index)
                surface->insert_all_ledges_hitting_ray(
                    ray_solvers[index], object);
        }
    }

    void check_ray_group_against_node(
        IVP_OV_Node *node, IVP_OV_Tree_Manager *treeManager) {
        if (!node || !treeManager)
            return;
        IVP_U_Float_Point minimum;
        IVP_FLOAT cubeSize = 0.0f;
        treeManager->get_luf_coordinates_ws(node, &minimum, &cubeSize);
        const IVP_FLOAT halfSize = cubeSize * 0.5f;
        IVP_U_Float_Point center(
            minimum.k[0] + halfSize,
            minimum.k[1] + halfSize,
            minimum.k[2] + halfSize);
        if (check_ray_group_against_cube(&center, cubeSize) == IVP_FALSE)
            return;

        for (int index = node->elements.len() - 1; index >= 0; --index) {
            IVP_OV_Element *element = node->elements.element_at(index);
            IVP_Real_Object *object = element ? element->real_object : nullptr;
            IVP_Core *core = object ? object->get_core() : nullptr;
            if (!core)
                continue;
            IVP_U_Float_Point objectCenter(core->get_position_PSI());
            if (check_ray_group_against_sphere(
                    &objectCenter, core->upper_limit_radius) != IVP_FALSE) {
                check_ray_group_against_object(object);
            }
        }
        for (int index = node->children.len() - 1; index >= 0; --index)
            check_ray_group_against_node(
                node->children.element_at(index), treeManager);
    }

    void check_ray_group_against_all_objects_in_sim(
        const IVP_Environment *environment) {
        IVP_OV_Tree_Manager *treeManager =
            environment ? environment->get_ov_tree_manager() : nullptr;
        if (treeManager)
            check_ray_group_against_node(
                treeManager->root, treeManager);
    }

private:
    IVP_BOOL check_ray_group_against_sphere(
        const IVP_U_Float_Point *sphereCenterWs,
        IVP_FLOAT sphereRadius) const {
        IVP_U_Float_Point distance;
        distance.subtract(sphereCenterWs, &center_ws);
        const IVP_DOUBLE broadRadius = radius + sphereRadius;
        return distance.quad_length() < broadRadius * broadRadius
            ? IVP_TRUE
            : IVP_FALSE;
    }

    IVP_U_Float_Point center_ws;
    IVP_FLOAT radius;
    int n_ray_solvers;
    IVP_Ray_Solver **ray_solvers;
};

class IVP_Ray_Solver_Min_Hash final : public IVP_Ray_Solver {
public:
    explicit IVP_Ray_Solver_Min_Hash(
        const IVP_Ray_Solver_Template *configuration)
        : IVP_Ray_Solver(configuration), output_min_hash(8) {}
    ~IVP_Ray_Solver_Min_Hash() override = default;

    IVP_U_Min_Hash *get_result_min_hash() { return &output_min_hash; }

private:
    void add_hit_object(
        IVP_Real_Object *object, const IVP_Compact_Ledge *ledge,
        const IVP_Compact_Triangle *triangle, IVP_DOUBLE hitDistance,
        IVP_U_Point *surfaceDirectionObject) override {
        if (output_min_hash.counter >= IVP_MAX_NUM_RAY_HITS)
            return;
        IVP_Ray_Hit *hit = &hit_info[output_min_hash.counter];
        hit->hit_real_object = object;
        hit->hit_compact_ledge = ledge;
        hit->hit_compact_triangle = triangle;
        hit->hit_surface_direction_os.set(surfaceDirectionObject);
        hit->hit_distance = static_cast<IVP_FLOAT>(hitDistance);
        output_min_hash.add(hit, hitDistance);
    }

    IVP_U_Min_Hash output_min_hash;
    IVP_Ray_Hit hit_info[IVP_MAX_NUM_RAY_HITS];
};

class IVP_Ray_Solver_Os {
    friend class IVP_SurfaceManager_Grid;
    friend struct BML_IvpRaySolverOsLayoutCheck;

protected:
    IVP_U_Point ray_start_point;
    IVP_U_Float_Point ray_center_point;
    IVP_U_Point ray_end_point;
    IVP_U_Float_Point ray_direction;
    IVP_Ray_Hit_Listener *hit_listener;
    IVP_Real_Object *object;
    IVP_FLOAT ray_length;
    std::uint32_t reserved_6C;

public:
    IVP_Ray_Solver_Os(
        class IVP_Ray_Solver *solver, IVP_Real_Object *realObject) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RaySolverObjectConstruct,
            this, solver, realObject);
    }
    IVP_BOOL check_ray_against_compact_ledge_os(
        const IVP_Compact_Ledge *ledge) {
        if (!ledge || !hit_listener || ledge->get_n_triangles() <= 0)
            return IVP_FALSE;

        // Ballance retains this body at RVA 0x21AB0 but omits its public
        // decoration and does not materialize a reliable IVP_BOOL in EAX on
        // every exit. Call the retail body for its collision side effect and
        // recover the source-level Boolean contract by observing the exact
        // hit-listener callback. The forwarding listener keeps arbitrary Mod
        // listeners on the same two-slot ABI used by physics_RT.dll.
        using RetailFunction =
            void (__thiscall *)(void *, const IVP_Compact_Ledge *);
        if (BML::IVP::ABI::Resolve<RetailFunction>(
                BML::IVP::ABI::Address::RaySolverObjectCheckCompactLedge)) {
            class ForwardingListener final : public IVP_Ray_Hit_Listener {
            public:
                explicit ForwardingListener(IVP_Ray_Hit_Listener *target)
                    : target_(target) {}

                void add_hit_object(
                    IVP_Real_Object *hitObject,
                    const IVP_Compact_Ledge *hitLedge,
                    const IVP_Compact_Triangle *hitTriangle,
                    IVP_DOUBLE hitDistance,
                    IVP_U_Point *surfaceDirectionObject) override {
                    hit_ = IVP_TRUE;
                    target_->add_hit_object(
                        hitObject, hitLedge, hitTriangle, hitDistance,
                        surfaceDirectionObject);
                }

                IVP_BOOL hit() const { return hit_; }

            private:
                IVP_Ray_Hit_Listener *target_;
                IVP_BOOL hit_ = IVP_FALSE;
            } forwardingListener(hit_listener);

            struct ListenerRestore {
                IVP_Ray_Hit_Listener *&slot;
                IVP_Ray_Hit_Listener *original;
                ~ListenerRestore() { slot = original; }
            } restore{hit_listener, hit_listener};

            hit_listener = &forwardingListener;
            BML::IVP::ABI::InvokeThis<void>(
                BML::IVP::ABI::Address::RaySolverObjectCheckCompactLedge,
                this, ledge);
            return forwardingListener.hit();
        }

        auto calculateNormal = [ledge](
                                   const IVP_Compact_Edge *edge,
                                   IVP_U_Point *normal) {
            const IVP_U_Float_Point *point0 = edge->get_start_point(ledge);
            const IVP_U_Float_Point *point1 =
                edge->get_next()->get_start_point(ledge);
            const IVP_U_Float_Point *point2 =
                edge->get_prev()->get_start_point(ledge);
            normal->inline_set_vert_to_area_defined_by_three_points(
                point0, point2, point1);
        };

        auto isInsideTriangle = [ledge](
                                      const IVP_Compact_Edge *edge,
                                      const IVP_U_Point *point) {
            const IVP_U_Float_Point *point0 = edge->get_start_point(ledge);
            const IVP_U_Float_Point *point1 =
                edge->get_next()->get_start_point(ledge);
            const IVP_U_Float_Point *point2 =
                edge->get_prev()->get_start_point(ledge);

            IVP_U_Point qAxis;
            IVP_U_Point rAxis;
            IVP_U_Point offset;
            qAxis.subtract(point0, point1);
            rAxis.subtract(point2, point1);
            offset.subtract(point, point1);
            const IVP_DOUBLE qq = qAxis.quad_length();
            const IVP_DOUBLE rr = rAxis.quad_length();
            const IVP_DOUBLE qr = qAxis.dot_product(&rAxis);
            const IVP_DOUBLE determinant = qq * rr - qr * qr;
            if (determinant <= 0.0)
                return false;
            const IVP_DOUBLE sq = offset.dot_product(&qAxis);
            const IVP_DOUBLE sr = offset.dot_product(&rAxis);
            const IVP_DOUBLE q = rr * sq - sr * qr;
            const IVP_DOUBLE r = qq * sr - sq * qr;
            return q >= 0.0 && r >= 0.0 && determinant - q - r >= 0.0;
        };

        const int triangleCount = ledge->get_n_triangles();
        const IVP_Compact_Triangle *firstTriangle =
            ledge->get_first_triangle();

        // A two-triangle ledge represents one two-sided polygon. The retail
        // solver tests its first triangle and flips the reported normal when
        // the ray crosses the back side.
        if (triangleCount == 2) {
            const IVP_Compact_Edge *edge = firstTriangle->get_first_edge();
            IVP_U_Point normal;
            calculateNormal(edge, &normal);
            const IVP_U_Float_Point *point0 = edge->get_start_point(ledge);
            const IVP_DOUBLE planeOffset = normal.dot_product(point0);
            const IVP_DOUBLE startSide =
                ray_start_point.dot_product(&normal) - planeOffset;
            const IVP_DOUBLE endSide =
                ray_end_point.dot_product(&normal) - planeOffset;
            if (startSide * endSide >= 0.0)
                return IVP_FALSE;

            const IVP_DOUBLE fraction = startSide / (startSide - endSide);
            IVP_U_Point intersection;
            intersection.set_interpolate(
                &ray_start_point, &ray_end_point, fraction);
            if (!isInsideTriangle(edge, &intersection))
                return IVP_FALSE;
            if (startSide < 0.0)
                normal.mult(-1.0);
            if (normal.normize() != IVP_OK)
                return IVP_FALSE;
            hit_listener->add_hit_object(
                object, ledge, firstTriangle, fraction * ray_length,
                &normal);
            return IVP_TRUE;
        }

        const IVP_Compact_Triangle *bestTriangle = nullptr;
        IVP_U_Point bestNormal;
        IVP_DOUBLE bestFraction = 2.0;
        const IVP_Compact_Triangle *triangle = firstTriangle;
        for (int index = 0; index < triangleCount;
             ++index, triangle = triangle->get_next_tri()) {
            const IVP_Compact_Edge *edge = triangle->get_first_edge();
            IVP_U_Point normal;
            calculateNormal(edge, &normal);
            if (normal.dot_product(&ray_direction) > -1.0e-12)
                continue;

            const IVP_U_Float_Point *point0 = edge->get_start_point(ledge);
            const IVP_DOUBLE planeOffset = normal.dot_product(point0);
            const IVP_DOUBLE startSide =
                ray_start_point.dot_product(&normal) - planeOffset;
            if (startSide <= 0.0)
                continue;
            const IVP_DOUBLE endSide =
                ray_end_point.dot_product(&normal) - planeOffset;
            if (endSide > 0.0)
                continue;

            const IVP_DOUBLE fraction = startSide / (startSide - endSide);
            if (fraction >= bestFraction)
                continue;
            IVP_U_Point intersection;
            intersection.set_interpolate(
                &ray_start_point, &ray_end_point, fraction);
            if (!isInsideTriangle(edge, &intersection))
                continue;
            bestTriangle = triangle;
            bestNormal = normal;
            bestFraction = fraction;
        }

        if (!bestTriangle || bestNormal.normize() != IVP_OK)
            return IVP_FALSE;
        hit_listener->add_hit_object(
            object, ledge, bestTriangle, bestFraction * ray_length,
            &bestNormal);
        return IVP_TRUE;
    }
    void check_ray_against_ledge_tree_node_os(
        const IVP_Compact_Ledgetree_Node *node) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RaySolverObjectCheckLedgeTree,
            this, node);
    }
    void check_ray_against_compact_surface_os(
        const IVP_Compact_Surface *surface) {
        BML::IVP::ABI::InvokeThis<void>(
            BML::IVP::ABI::Address::RaySolverObjectCheckCompactSurface,
            this, surface);
    }
};

inline void IVP_Ray_Solver::check_ray_against_compact_ledge_os(
    const IVP_Compact_Ledge *ledge, IVP_Real_Object *object) {
    if (!ledge || !object)
        return;
    IVP_Ray_Solver_Os objectSpaceSolver(this, object);
    objectSpaceSolver.check_ray_against_compact_ledge_os(ledge);
}

// Header-side collector deliberately uses a Mod-owned vtable. The retail
// surface managers call the two-slot IVP_Ray_Hit_Listener ABI above.
class IVP_Ray_Solver_Min final : public IVP_Ray_Solver {
    friend struct BML_IvpRaySolverMinLayoutCheck;

public:
    explicit IVP_Ray_Solver_Min(const IVP_Ray_Solver_Template *configuration)
        : IVP_Ray_Solver(configuration), min_dist(1.0e16f) {}
    ~IVP_Ray_Solver_Min() override = default;

    IVP_Ray_Hit *get_ray_hit() {
        return min_dist == 1.0e16f ? nullptr : &ray_hit;
    }
    const IVP_Ray_Hit *get_ray_hit() const {
        return min_dist == 1.0e16f ? nullptr : &ray_hit;
    }
    IVP_FLOAT get_ray_dist() { return min_dist; }
    IVP_FLOAT get_ray_dist() const { return min_dist; }

    void add_hit_object(IVP_Real_Object *object,
                        const IVP_Compact_Ledge *ledge,
                        const IVP_Compact_Triangle *triangle,
                        IVP_DOUBLE hitDistance,
                        IVP_U_Point *surfaceDirectionObject) override {
        if (hitDistance >= min_dist)
            return;
        min_dist = static_cast<IVP_FLOAT>(hitDistance);
        ray_hit.hit_real_object = object;
        ray_hit.hit_compact_ledge = ledge;
        ray_hit.hit_compact_triangle = triangle;
        ray_hit.hit_distance = static_cast<IVP_FLOAT>(hitDistance);
        ray_hit.hit_surface_direction_os.set(surfaceDirectionObject);
    }

protected:
    IVP_FLOAT min_dist;
    IVP_Ray_Hit ray_hit;
};

struct BML_IvpRaySolverOsLayoutCheck {
    static constexpr std::size_t start =
        offsetof(IVP_Ray_Solver_Os, ray_start_point);
    static constexpr std::size_t center =
        offsetof(IVP_Ray_Solver_Os, ray_center_point);
    static constexpr std::size_t end =
        offsetof(IVP_Ray_Solver_Os, ray_end_point);
    static constexpr std::size_t direction =
        offsetof(IVP_Ray_Solver_Os, ray_direction);
    static constexpr std::size_t listener =
        offsetof(IVP_Ray_Solver_Os, hit_listener);
    static constexpr std::size_t object =
        offsetof(IVP_Ray_Solver_Os, object);
    static constexpr std::size_t length =
        offsetof(IVP_Ray_Solver_Os, ray_length);
};

struct BML_IvpRaySolverMinLayoutCheck {
    static constexpr std::size_t distance =
        offsetof(IVP_Ray_Solver_Min, min_dist);
    static constexpr std::size_t hit = offsetof(IVP_Ray_Solver_Min, ray_hit);
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Ray_Hit) == 0x20);
static_assert(sizeof(IVP_Ray_Solver_Template) == 0x38);
static_assert(sizeof(IVP_Ray_Hit_Listener) == 0x04);
static_assert(sizeof(IVP_Ray_Solver) == 0x70);
static_assert(offsetof(IVP_Ray_Solver, ray_start_point) == 0x08);
static_assert(offsetof(IVP_Ray_Solver, ray_length) == 0x68);
static_assert(sizeof(IVP_Ray_Solver_Os) == 0x70);
static_assert(BML_IvpRaySolverOsLayoutCheck::start == 0x00);
static_assert(BML_IvpRaySolverOsLayoutCheck::center == 0x20);
static_assert(BML_IvpRaySolverOsLayoutCheck::end == 0x30);
static_assert(BML_IvpRaySolverOsLayoutCheck::direction == 0x50);
static_assert(BML_IvpRaySolverOsLayoutCheck::listener == 0x60);
static_assert(BML_IvpRaySolverOsLayoutCheck::object == 0x64);
static_assert(BML_IvpRaySolverOsLayoutCheck::length == 0x68);
static_assert(sizeof(IVP_Ray_Solver_Group) == 0x1C);
static_assert(sizeof(IVP_Ray_Solver_Min_Hash) == 0x2088);
static_assert(sizeof(IVP_Ray_Solver_Min) == 0x98);
static_assert(BML_IvpRaySolverMinLayoutCheck::distance == 0x70);
static_assert(BML_IvpRaySolverMinLayoutCheck::hit == 0x74);
#endif

#endif // BML_IVP_RAY_H
