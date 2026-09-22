#include "IvpRetailTestSupport.h"

TEST(IvpRetailPolygonTemplate,
     OriginalTransportLayoutIndexesSharedTopologyWithoutCopies) {
    static_assert(sizeof(void *) == 4,
                  "The Ballance retail physics ABI is 32-bit x86");

    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    IVP_Template_Polygon polygon;
    EXPECT_EQ(polygon.n_points, 0);
    EXPECT_EQ(polygon.points, nullptr);
    EXPECT_EQ(polygon.n_lines, 0);
    EXPECT_EQ(polygon.lines, nullptr);
    EXPECT_EQ(polygon.n_surfaces, 0);
    EXPECT_EQ(polygon.surfaces, nullptr);

    // A convex tetrahedron shares four points and six edge records between
    // its four faces.  This checks the old polygon transport contract that
    // Ballance's retained convex builder consumes, including the retail
    // pointer-difference implementation of get_surface_index().
    std::array<IVP_Template_Point, 4> points{};
    points[0].set(0.0, 0.0, 0.0);
    points[1].set(1.0, 0.0, 0.0);
    points[2].set(0.0, 1.0, 0.0);
    points[3].set(0.0, 0.0, 1.0);
    std::array<IVP_Template_Line, 6> lines{};
    lines[0].set(0, 1);
    lines[1].set(0, 2);
    lines[2].set(0, 3);
    lines[3].set(1, 2);
    lines[4].set(1, 3);
    lines[5].set(2, 3);
    std::array<IVP_Template_Surface, 4> surfaces{};

    polygon.n_points = static_cast<int>(points.size());
    polygon.points = points.data();
    polygon.n_lines = static_cast<int>(lines.size());
    polygon.lines = lines.data();
    polygon.n_surfaces = static_cast<int>(surfaces.size());
    polygon.surfaces = surfaces.data();
    for (auto &surface : surfaces)
        surface.templ_poly = &polygon;

    EXPECT_EQ(surfaces[0].get_surface_index(), 0);
    EXPECT_EQ(surfaces[1].get_surface_index(), 1);
    EXPECT_EQ(surfaces[2].get_surface_index(), 2);
    EXPECT_EQ(surfaces[3].get_surface_index(), 3);
    EXPECT_EQ(polygon.lines[4].p[0], 1u);
    EXPECT_EQ(polygon.lines[4].p[1], 3u);

    // The exact polygon destructor owns only heap arrays. This fixture uses
    // stack storage deliberately, so detach the borrowed topology first.
    polygon.n_points = polygon.n_lines = polygon.n_surfaces = 0;
    polygon.points = nullptr;
    polygon.lines = nullptr;
    polygon.surfaces = nullptr;
}

TEST(IvpRetailPointsoupBuilder,
     OriginalHullBuilderProducesBallanceCollisionGeometry) {
    static_assert(sizeof(void *) == 4,
                  "The Ballance retail physics ABI is 32-bit x86");
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    IVP_U_Point p0(0.0, 0.0, 0.0);
    IVP_U_Point p1(3.0, 0.0, 0.0);
    IVP_U_Point p2(3.0, 4.0, 0.0);
    IVP_U_Point p3(0.0, 4.0, 0.0);

    IVP_SurMan_PS_Plane plane;
    plane.set(0.0, 0.0, -1.0);
    plane.points.add(&p0);
    plane.points.add(&p1);
    plane.points.add(&p2);
    plane.points.add(&p3);
    EXPECT_DOUBLE_EQ(plane.get_area_size(), 24.0);
    EXPECT_DOUBLE_EQ(plane.get_qlen_of_all_edges(), 50.0);

    IVP_U_Point triangle2(0.0, 4.0, 0.0);
    IVP_Compact_Ledge *triangle =
        IVP_SurfaceBuilder_Pointsoup::convert_triangle_to_compace_ledge(
            &p0, &p1, &triangle2);
    ASSERT_NE(triangle, nullptr);
    EXPECT_EQ(triangle->get_n_points(), 3);
    EXPECT_TRUE(triangle->is_terminal());
    EXPECT_FLOAT_EQ(triangle->get_point_array()[0].k[0], 0.0f);
    EXPECT_FLOAT_EQ(triangle->get_point_array()[1].k[0], 3.0f);
    EXPECT_FLOAT_EQ(triangle->get_point_array()[2].k[1], 4.0f);
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, triangle);

    IVP_U_Point tetraTop(0.0, 0.0, 2.0);
    IVP_Vector_of_Points_256 tetrahedron;
    tetrahedron.add(&p0);
    tetrahedron.add(&p1);
    tetrahedron.add(&triangle2);
    tetrahedron.add(&tetraTop);

    IVP_Compact_Ledge *hull =
        IVP_SurfaceBuilder_Pointsoup::convert_pointsoup_to_compact_ledge(
            &tetrahedron);
    ASSERT_NE(hull, nullptr);
    EXPECT_EQ(hull->get_n_points(), 4);
    EXPECT_TRUE(hull->is_terminal());

    IVP_Template_Compact_Grid gridTemplate;
    EXPECT_EQ(gridTemplate.row_info.n_points, 0);
    EXPECT_EQ(gridTemplate.column_info.n_points, 0);
    EXPECT_FLOAT_EQ(gridTemplate.grid_field_size, 0.0f);

    // Assemble the documented variable-size retail grid format from two
    // copies of an exact-DLL compact ledge. This models adjacent terrain cells
    // without substituting a foreign heightfield compiler.
    IVP_Compact_Ledge *gridTriangle =
        IVP_SurfaceBuilder_Pointsoup::convert_triangle_to_compace_ledge(
            &p0, &p1, &triangle2);
    ASSERT_NE(gridTriangle, nullptr);
    ASSERT_EQ(gridTriangle->get_n_triangles(), 2);
    const int ledgeSize = gridTriangle->get_size();
    constexpr int elementOffset = sizeof(IVP_Compact_Grid);
    constexpr int ledgeOffset0 = elementOffset +
        4 * sizeof(IVP_Compact_Grid_Element);
    const int ledgeOffset1 = ledgeOffset0 + ledgeSize;
    const int gridSize = ledgeOffset1 + ledgeSize;
    auto *grid = static_cast<IVP_Compact_Grid *>(
        BML::IVP::ABI::Invoke<void *>(
            BML::IVP::ABI::Address::AllocateAligned, gridSize, 16));
    ASSERT_NE(grid, nullptr);
    std::memset(grid, 0, static_cast<std::size_t>(gridSize));
    grid->center.set(1.0f, 0.5f, 0.0f);
    grid->m_grid_f_object.set_identity();
    grid->n_rows = 2;
    grid->n_columns = 2;
    grid->n_compact_ledges = 2;
    grid->radius = 3.0f;
    grid->byte_size = gridSize;
    grid->inv_grid_size = 1.0f;
    grid->offset_grid_elements = elementOffset;
    grid->offset_compact_ledge_array[0] = ledgeOffset0;
    grid->offset_compact_ledge_array[1] = ledgeOffset1;
    auto *gridElements = const_cast<IVP_Compact_Grid_Element *>(
        grid->get_grid_elements());
    for (int index = 0; index < 4; ++index) {
        gridElements[index].compact_ledge_index[0] = -1;
        gridElements[index].compact_ledge_index[1] = -1;
    }
    gridElements[0].compact_ledge_index[0] = 0;
    gridElements[0].compact_ledge_index[1] = 0; // duplicate cell reference
    gridElements[1].compact_ledge_index[0] = 1;
    std::memcpy(reinterpret_cast<std::byte *>(grid) + ledgeOffset0,
                gridTriangle, static_cast<std::size_t>(ledgeSize));
    std::memcpy(reinterpret_cast<std::byte *>(grid) + ledgeOffset1,
                gridTriangle, static_cast<std::size_t>(ledgeSize));
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, gridTriangle);

    IVP_SurfaceManager_Grid gridManager(grid);
    EXPECT_EQ(gridManager.get_compact_grid(), grid);
    EXPECT_EQ(gridManager.get_type(), IVP_SURMAN_POLYGON);
    EXPECT_EQ(gridManager.get_single_convex(), nullptr);
    gridManager.add_reference_to_ledge(grid->get_compact_ledge_at(0));
    gridManager.remove_reference_to_ledge(grid->get_compact_ledge_at(0));
    IVP_U_Float_Point gridCenter;
    IVP_U_Float_Point gridInertia;
    gridManager.get_mass_center(&gridCenter);
    gridManager.get_rotation_inertia(&gridInertia);
    EXPECT_FLOAT_EQ(gridCenter.k[0], 1.0f);
    EXPECT_FLOAT_EQ(gridInertia.k[0], 9.0f);
    IVP_FLOAT gridRadius = 0.0f;
    IVP_FLOAT gridRadiusDeviation = 0.0f;
    gridManager.get_radius_and_radius_dev_to_given_center(
        &gridCenter, &gridRadius, &gridRadiusDeviation);
    EXPECT_FLOAT_EQ(gridRadius, 3.0f);
    EXPECT_FLOAT_EQ(gridRadiusDeviation, 3.0f);

    IVP_U_Point firstCell(0.1, 0.1, 0.0);
    IVP_U_BigVector<IVP_Compact_Ledge> nearbyLedges;
    gridManager.get_all_ledges_within_radius(
        &firstCell, 0.1, nullptr, nullptr, nullptr, &nearbyLedges);
    ASSERT_EQ(nearbyLedges.len(), 1);
    EXPECT_EQ(nearbyLedges.element_at(0),
              grid->get_compact_ledge_at(0));

    IVP_U_Point aboveFirstCell(0.1, 0.1, 10.0);
    IVP_U_BigVector<IVP_Compact_Ledge> distantLedges;
    gridManager.get_all_ledges_within_radius(
        &aboveFirstCell, 0.1, nullptr, nullptr, nullptr, &distantLedges);
    EXPECT_EQ(distantLedges.len(), 0);

    IVP_U_BigVector<IVP_Compact_Ledge> allGridLedges;
    gridManager.get_all_terminal_ledges(&allGridLedges);
    ASSERT_EQ(allGridLedges.len(), 2);
    EXPECT_EQ(allGridLedges.element_at(0),
              grid->get_compact_ledge_at(0));
    EXPECT_EQ(allGridLedges.element_at(1),
              grid->get_compact_ledge_at(1));
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, grid);

    // Compile a Ballance-style heightfield rather than only exercising a
    // hand-assembled format. Rows run toward -Y, columns toward +X, and the
    // height axis starts at a non-zero object-space origin.
    IVP_Template_Compact_Grid terrainTemplate;
    terrainTemplate.row_info.n_points = 3;
    terrainTemplate.row_info.maps_to = IVP_INDEX_Y;
    terrainTemplate.row_info.invert_axis = IVP_TRUE;
    terrainTemplate.column_info.n_points = 3;
    terrainTemplate.column_info.maps_to = IVP_INDEX_X;
    terrainTemplate.column_info.invert_axis = IVP_FALSE;
    terrainTemplate.height_maps_to = IVP_INDEX_Z;
    terrainTemplate.height_invert_axis = IVP_FALSE;
    terrainTemplate.grid_field_size = 2.0f;
    terrainTemplate.position_origin_os.set(10.0f, 20.0f, 5.0f);
    std::array<IVP_FLOAT, 9> terrainHeights{
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 2.0f,
        0.0f, 2.0f, 4.0f,
    };
    IVP_Compact_Grid *terrain =
        IVP_GridBuilder_Array::convert_array_to_compact_grid(
            nullptr, &terrainTemplate, terrainHeights.data());
    ASSERT_NE(terrain, nullptr);
    EXPECT_EQ(terrain->n_rows, 2);
    EXPECT_EQ(terrain->n_columns, 2);
    EXPECT_EQ(terrain->n_compact_ledges, 8);
    EXPECT_FLOAT_EQ(terrain->inv_grid_size, 0.5f);
    EXPECT_FLOAT_EQ(terrain->center.k[0], 12.0f);
    EXPECT_FLOAT_EQ(terrain->center.k[1], 18.0f);
    EXPECT_FLOAT_EQ(terrain->center.k[2], 7.0f);
    EXPECT_NEAR(terrain->radius, std::sqrt(12.0f), 1.0e-5f);
    const IVP_Compact_Grid_Element *terrainElements =
        terrain->get_grid_elements();
    for (int cell = 0; cell < 4; ++cell) {
        EXPECT_EQ(terrainElements[cell].compact_ledge_index[0], cell * 2);
        EXPECT_EQ(terrainElements[cell].compact_ledge_index[1], cell * 2 + 1);
    }
    bool foundOrigin = false;
    const IVP_Compact_Ledge *firstTerrainLedge =
        terrain->get_compact_ledge_at(0);
    for (int pointIndex = 0;
         pointIndex < firstTerrainLedge->get_n_points(); ++pointIndex) {
        const IVP_Compact_Poly_Point &point =
            firstTerrainLedge->get_point_array()[pointIndex];
        if (point.k[0] == 10.0f && point.k[1] == 20.0f &&
            point.k[2] == 5.0f)
            foundOrigin = true;
    }
    EXPECT_TRUE(foundOrigin);

    IVP_SurfaceManager_Grid terrainManager(terrain);
    IVP_U_Point firstTerrainCell(10.5, 19.5, 5.2);
    IVP_U_BigVector<IVP_Compact_Ledge> terrainNearby;
    terrainManager.get_all_ledges_within_radius(
        &firstTerrainCell, 0.8, nullptr, nullptr, nullptr, &terrainNearby);
    ASSERT_EQ(terrainNearby.len(), 2);
    EXPECT_EQ(terrainNearby.element_at(0), terrain->get_compact_ledge_at(0));
    EXPECT_EQ(terrainNearby.element_at(1), terrain->get_compact_ledge_at(1));
    IVP_U_BigVector<IVP_Compact_Ledge> allTerrainLedges;
    terrainManager.get_all_terminal_ledges(&allTerrainLedges);
    EXPECT_EQ(allTerrainLedges.len(), 8);
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, terrain);

    // Build two gameplay collision faces from indexed mesh topology. This is
    // the useful stripped path for imported ramps and platforms; each face is
    // compiled by the original Ballance Pointsoup implementation.
    IVP_Concave_Polyhedron faceSoup;
    faceSoup.points.add(&p0);
    faceSoup.points.add(&p1);
    faceSoup.points.add(&triangle2);
    faceSoup.points.add(&tetraTop);
    IVP_Concave_Polyhedron_Face floorFace;
    floorFace.add_offset(0);
    floorFace.add_offset(1);
    floorFace.add_offset(2);
    floorFace.add_offset(2); // public contract drops duplicate indices
    IVP_Concave_Polyhedron_Face rampFace;
    rampFace.add_offset(0);
    rampFace.add_offset(1);
    rampFace.add_offset(3);
    faceSoup.faces.add(&floorFace);
    faceSoup.faces.add(&rampFace);
    IVP_U_BigVector<IVP_Compact_Ledge> faceLedges;
    IVP_SurfaceBuilder_Polyhedron_Concave::
        convert_concave_face_soup_to_compact_ledges(
            &faceSoup, &faceLedges);
    ASSERT_EQ(floorFace.point_offset.len(), 3);
    ASSERT_EQ(faceLedges.len(), 2);
    for (int index = 0; index < faceLedges.len(); ++index) {
        ASSERT_NE(faceLedges.element_at(index), nullptr);
        EXPECT_EQ(faceLedges.element_at(index)->get_n_points(), 3);
        EXPECT_TRUE(faceLedges.element_at(index)->is_terminal());
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::FreeAligned,
            faceLedges.element_at(index));
    }

    // A closed platform with a shallow inward roof dent is concave but
    // star-shaped around its vertex centroid. Decompose it into tetrahedra,
    // compile every piece with the original Pointsoup implementation, and
    // combine the pieces with the original Ledge Soup implementation.
    std::array<IVP_U_Point, 9> dentedPlatformPoints{
        IVP_U_Point(-1.0, -1.0, 0.0), IVP_U_Point(1.0, -1.0, 0.0),
        IVP_U_Point(1.0, 1.0, 0.0), IVP_U_Point(-1.0, 1.0, 0.0),
        IVP_U_Point(-1.0, -1.0, 2.0), IVP_U_Point(1.0, -1.0, 2.0),
        IVP_U_Point(1.0, 1.0, 2.0), IVP_U_Point(-1.0, 1.0, 2.0),
        IVP_U_Point(0.0, 0.0, 1.8),
    };
    std::array<IVP_Concave_Polyhedron_Face, 14> dentedPlatformFaces;
    const std::array<std::array<int, 3>, 14> dentedPlatformIndices{{
        {{0, 2, 1}}, {{0, 3, 2}},
        {{0, 1, 5}}, {{0, 5, 4}},
        {{1, 2, 6}}, {{1, 6, 5}},
        {{2, 3, 7}}, {{2, 7, 6}},
        {{3, 0, 4}}, {{3, 4, 7}},
        {{4, 5, 8}}, {{5, 6, 8}}, {{6, 7, 8}}, {{7, 4, 8}},
    }};
    IVP_Concave_Polyhedron dentedPlatform;
    for (IVP_U_Point &point : dentedPlatformPoints)
        dentedPlatform.points.add(&point);
    for (std::size_t faceIndex = 0;
         faceIndex < dentedPlatformFaces.size(); ++faceIndex) {
        for (int pointIndex : dentedPlatformIndices[faceIndex])
            dentedPlatformFaces[faceIndex].add_offset(pointIndex);
        dentedPlatform.faces.add(&dentedPlatformFaces[faceIndex]);
    }
    IVP_Convex_Decompositor_Parameters decompositionParameters;
    decompositionParameters.tolin = 1.0e-5f;
    decompositionParameters.angacc = 0.001f;
    decompositionParameters.rdacc = 0.001f;
    IVP_U_BigVector<IVP_Convex_Subpart> platformSubparts;
    ASSERT_EQ(IVP_Convex_Decompositor::
                  perform_convex_decomposition_on_concave_polyhedron(
                      &dentedPlatform, &decompositionParameters,
                      &platformSubparts),
              14);
    ASSERT_EQ(platformSubparts.len(), 14);
    for (int index = 0; index < platformSubparts.len(); ++index) {
        EXPECT_EQ(platformSubparts.element_at(index)->points.len(), 4);
        delete platformSubparts.element_at(index);
    }

    IVP_U_BigVector<IVP_Compact_Ledge> platformLedges;
    EXPECT_EQ(IVP_SurfaceBuilder_Polyhedron_Concave::
                  convert_concave_polyhedron_to_compact_ledges(
                      &dentedPlatform, &decompositionParameters,
                      &platformLedges),
              14);
    ASSERT_EQ(platformLedges.len(), 14);
    for (int index = 0; index < platformLedges.len(); ++index) {
        EXPECT_EQ(platformLedges.element_at(index)->get_n_points(), 4);
        EXPECT_TRUE(platformLedges.element_at(index)->is_terminal());
        BML::IVP::ABI::Invoke<void>(
            BML::IVP::ABI::Address::FreeAligned,
            platformLedges.element_at(index));
    }

    IVP_Compact_Surface *platformSurface =
        IVP_SurfaceBuilder_Polyhedron_Concave::
            convert_concave_polyhedron_to_single_compact_surface(
                &dentedPlatform, &decompositionParameters);
    ASSERT_NE(platformSurface, nullptr);
    EXPECT_GT(platformSurface->get_size(),
              static_cast<int>(sizeof(IVP_Compact_Surface)));
    IVP_SurfaceManager_Polygon platformManager(platformSurface);
    IVP_U_BigVector<IVP_Compact_Ledge> compiledPlatformLedges;
    platformManager.get_all_terminal_ledges(&compiledPlatformLedges);
    EXPECT_EQ(compiledPlatformLedges.len(), 14);
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, platformSurface);

    // Moving the dent below the platform floor puts the arithmetic center
    // outside at least one outward face. The conservative decompositor must
    // reject that non-star-shaped volume instead of emitting overlapping or
    // inverted collision tetrahedra.
    dentedPlatformPoints[8].k[2] = -1.0;
    IVP_U_BigVector<IVP_Convex_Subpart> unsupportedSubparts;
    EXPECT_EQ(IVP_Convex_Decompositor::
                  perform_convex_decomposition_on_concave_polyhedron(
                      &dentedPlatform, &decompositionParameters,
                      &unsupportedSubparts),
              0);
    EXPECT_EQ(unsupportedSubparts.len(), 0);
    dentedPlatformPoints[8].k[2] = 1.8;

    // Convert the exact Ballance compact hull into inward-facing support
    // planes and back again. This is the geometry path used when a Mod clips
    // or rebuilds a convex collision volume.
    IVP_Halfspacesoup halfspaces(hull);
    ASSERT_EQ(halfspaces.len(), 4);
    IVP_U_Point interior(0.75, 1.0, 0.5);
    IVP_U_Point exterior(10.0, 10.0, 10.0);
    bool exteriorRejected = false;
    for (int index = 0; index < halfspaces.len(); ++index) {
        EXPECT_GE(halfspaces.element_at(index)->get_dist(&interior), -1.0e-5);
        if (halfspaces.element_at(index)->get_dist(&exterior) < -1.0e-4)
            exteriorRejected = true;
    }
    EXPECT_TRUE(exteriorRejected);

    IVP_Halfspacesoup copiedHalfspaces;
    for (int index = 0; index < halfspaces.len(); ++index)
        copiedHalfspaces.add_halfspace(halfspaces.element_at(index));
    ASSERT_EQ(copiedHalfspaces.len(), 4);
    IVP_U_Hesse looserPlane(*copiedHalfspaces.element_at(0));
    looserPlane.hesse_val += 1.0;
    copiedHalfspaces.add_halfspace(&looserPlane);
    EXPECT_EQ(copiedHalfspaces.len(), 4);
    IVP_U_Hesse tighterPlane(*copiedHalfspaces.element_at(0));
    tighterPlane.hesse_val -= 0.25;
    copiedHalfspaces.add_halfspace(&tighterPlane);
    EXPECT_EQ(copiedHalfspaces.len(), 4);

    IVP_U_Vector<IVP_U_Point> recoveredPoints;
    ASSERT_EQ(IVP_SurfaceBuilder_Halfspacesoup::
                  convert_halfspacesoup_to_points(
                      &halfspaces, 0.001, &recoveredPoints),
              4);
    const IVP_U_Point expectedPoints[] = {p0, p1, triangle2, tetraTop};
    for (const IVP_U_Point &expected : expectedPoints) {
        bool found = false;
        for (int index = 0; index < recoveredPoints.len(); ++index) {
            if (expected.quad_distance_to(recoveredPoints.element_at(index)) <
                1.0e-8) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found);
    }
    for (int index = recoveredPoints.len() - 1; index >= 0; --index)
        delete recoveredPoints.element_at(index);
    recoveredPoints.remove_all();

    IVP_Compact_Ledge *recoveredHull =
        IVP_SurfaceBuilder_Halfspacesoup::
            convert_halfspacesoup_to_compact_ledge(&halfspaces, 0.001);
    ASSERT_NE(recoveredHull, nullptr);
    EXPECT_EQ(recoveredHull->get_n_points(), 4);
    EXPECT_EQ(recoveredHull->get_n_triangles(), 4);
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, recoveredHull);

    IVP_Compact_Surface *recoveredSurface =
        IVP_SurfaceBuilder_Halfspacesoup::
            convert_halfspacesoup_to_compact_surface(&halfspaces, 0.001);
    ASSERT_NE(recoveredSurface, nullptr);
    EXPECT_GT(recoveredSurface->get_size(),
              static_cast<int>(sizeof(IVP_Compact_Surface)));
    EXPECT_NE(recoveredSurface->get_compact_ledge_tree_root(), nullptr);

    IVP_Compact_Ledge *shrunkenHull = IVP_Compact_Modify::shrink(
        hull, 0.1f, 0.001);
    ASSERT_NE(shrunkenHull, nullptr);
    EXPECT_EQ(shrunkenHull->get_n_points(), 4);
    for (int pointIndex = 0; pointIndex < shrunkenHull->get_n_points();
         ++pointIndex) {
        for (int planeIndex = 0; planeIndex < halfspaces.len(); ++planeIndex) {
            EXPECT_GE(halfspaces.element_at(planeIndex)->get_dist(
                          &shrunkenHull->get_point_array()[pointIndex]),
                      0.095);
        }
    }
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, shrunkenHull);

    IVP_Compact_Surface *shrunkenSurface = IVP_Compact_Modify::shrink(
        recoveredSurface, 0.1f, 0.001);
    ASSERT_NE(shrunkenSurface, nullptr);
    {
        IVP_SurfaceManager_Polygon shrunkenManager(shrunkenSurface);
        IVP_U_BigVector<IVP_Compact_Ledge> shrunkenLedges;
        shrunkenManager.get_all_terminal_ledges(&shrunkenLedges);
        ASSERT_EQ(shrunkenLedges.len(), 1);
        EXPECT_EQ(shrunkenLedges.element_at(0)->get_n_points(), 4);
    }
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, shrunkenSurface);

    IVP_U_Float_Point chopDirection(1.0f, 0.0f, 0.0f);
    IVP_Compact_Surface *choppedSurface = IVP_Compact_Modify::chop(
        recoveredSurface, &chopDirection, 0.5f);
    ASSERT_NE(choppedSurface, nullptr);
    {
        IVP_SurfaceManager_Polygon choppedManager(choppedSurface);
        IVP_U_BigVector<IVP_Compact_Ledge> choppedLedges;
        choppedManager.get_all_terminal_ledges(&choppedLedges);
        ASSERT_EQ(choppedLedges.len(), 1);
        const IVP_Compact_Ledge *chopped = choppedLedges.element_at(0);
        IVP_FLOAT minimumX = chopped->get_point_array()[0].k[0];
        IVP_FLOAT maximumX = minimumX;
        for (int index = 1; index < chopped->get_n_points(); ++index) {
            minimumX = (std::min)(minimumX,
                chopped->get_point_array()[index].k[0]);
            maximumX = (std::max)(maximumX,
                chopped->get_point_array()[index].k[0]);
        }
        EXPECT_NEAR(minimumX, 0.5f, 1.0e-4f);
        EXPECT_NEAR(maximumX, 3.0f, 1.0e-4f);
    }
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, choppedSurface);

    IVP_U_Float_Point zeroDirection;
    zeroDirection.set_to_zero();
    EXPECT_EQ(IVP_Compact_Modify::chop(
                  recoveredSurface, &zeroDirection, 0.5f),
              nullptr);

    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, recoveredSurface);

    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, hull);

    IVP_Compact_Surface *surface =
        IVP_SurfaceBuilder_Pointsoup::convert_pointsoup_to_compact_surface(
            &tetrahedron);
    ASSERT_NE(surface, nullptr);
    EXPECT_GT(surface->get_size(),
              static_cast<int>(sizeof(IVP_Compact_Surface)));
    EXPECT_EQ(surface->byte_size, surface->get_size());
    EXPECT_EQ(surface->max_factor_surface_deviation,
              surface->get_max_factor_surface_deviation());
    EXPECT_EQ(surface->get_byte_size(), surface->get_size());
    // Tetrahedron with axis intercepts 3, 4 and 2. The retained Ballance
    // builder writes its volume centroid and its own hull inertia coefficients
    // into the compact-surface header. These coefficients deliberately lock
    // the retail result; they are not the analytic object-axis diagonal tensor.
    EXPECT_NEAR(surface->mass_center.k[0], 0.75f, 1.0e-5f);
    EXPECT_NEAR(surface->mass_center.k[1], 1.0f, 1.0e-5f);
    EXPECT_NEAR(surface->mass_center.k[2], 0.5f, 1.0e-5f);
    EXPECT_NEAR(surface->rotation_inertia.k[0], 0.6184659f, 1.0e-5f);
    EXPECT_NEAR(surface->rotation_inertia.k[1], 0.3693322f, 1.0e-5f);
    EXPECT_NEAR(surface->rotation_inertia.k[2], 0.6884086f, 1.0e-5f);
    EXPECT_GE(surface->upper_limit_radius,
              std::sqrt(0.75f * 0.75f + 3.0f * 3.0f + 0.5f * 0.5f));
    EXPECT_LT(surface->upper_limit_radius, 4.0f);
    EXPECT_NE(surface->get_compact_ledge_tree_root(), nullptr);
    EXPECT_EQ(surface->get_ledgetree_root(),
              surface->get_compact_ledge_tree_root());
    EXPECT_GE(surface->offset_ledgetree_root,
              static_cast<int>(sizeof(IVP_Compact_Surface)));
    EXPECT_EQ(surface->reserved[0], 0u);
    EXPECT_EQ(surface->reserved[1], 0u);
    EXPECT_EQ(surface->reserved[2], 0u);

    // Serialize an exact-DLL compact hull to the neighboring big-endian disk
    // representation. This validates shared-point de-duplication and the
    // packed edge/triangle/ledge fields, not merely the 0x30 surface header.
    const int serializedSize = surface->get_size();
    const int rootOffset = surface->offset_ledgetree_root;
    const IVP_Compact_Ledgetree_Node *nativeRoot =
        surface->get_compact_ledge_tree_root();
    const IVP_Compact_Ledge *nativeHull = nativeRoot->get_compact_hull();
    ASSERT_NE(nativeHull, nullptr);
    const int hullOffset = static_cast<int>(
        reinterpret_cast<const std::byte *>(nativeHull) -
        reinterpret_cast<const std::byte *>(surface));
    const int hullPointOffset = static_cast<int>(
        reinterpret_cast<const std::byte *>(nativeHull->get_point_array()) -
        reinterpret_cast<const std::byte *>(nativeHull));
    const int hullTriangleCount = nativeHull->get_n_triangles();
    const int hullPointCount = nativeHull->get_n_points();
    ASSERT_EQ(hullPointCount, 4);
    std::array<IVP_Compact_Poly_Point, 4> nativeHullPoints{};
    std::memcpy(nativeHullPoints.data(), nativeHull->get_point_array(),
                sizeof(nativeHullPoints));
    std::vector<std::byte> serialized(
        reinterpret_cast<const std::byte *>(surface),
        reinterpret_cast<const std::byte *>(surface) + serializedSize);
    auto *serializedSurface = reinterpret_cast<IVP_Compact_Surface *>(
        serialized.data());
    serializedSurface->byte_swap_all(IVP_TRUE, 8);

    auto readBigU32 = [](const void *address) {
        std::uint32_t stored = 0;
        std::memcpy(&stored, address, sizeof(stored));
        return BML::IVP::Detail::ByteSwap32(stored);
    };
    auto readBigU16 = [](const void *address) {
        std::uint16_t stored = 0;
        std::memcpy(&stored, address, sizeof(stored));
        return BML::IVP::Detail::ByteSwap16(stored);
    };
    auto readBigFloat = [&](const void *address) {
        const std::uint32_t bits = readBigU32(address);
        IVP_FLOAT value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    EXPECT_FLOAT_EQ(readBigFloat(serialized.data() +
                        offsetof(IVP_Compact_Surface, mass_center)),
                    surface->mass_center.k[0]);
    EXPECT_EQ(readBigU32(serialized.data() +
                  offsetof(IVP_Compact_Surface, offset_ledgetree_root)),
              static_cast<std::uint32_t>(rootOffset));
    const std::byte *serializedRoot = serialized.data() + rootOffset;
    EXPECT_EQ(readBigU32(serializedRoot),
              static_cast<std::uint32_t>(nativeRoot->offset_right_node));
    EXPECT_EQ(readBigU32(serializedRoot + sizeof(std::int32_t)),
              static_cast<std::uint32_t>(nativeRoot->offset_compact_ledge));

    const std::byte *serializedHull = serialized.data() + hullOffset;
    EXPECT_EQ(readBigU32(serializedHull),
              static_cast<std::uint32_t>(hullPointOffset));
    EXPECT_EQ(readBigU32(serializedHull + sizeof(std::int32_t)),
              static_cast<std::uint32_t>(nativeHull->get_client_data()));
    const std::uint32_t serializedLedgeFields =
        readBigU32(serializedHull + 2 * sizeof(std::int32_t));
    EXPECT_EQ((serializedLedgeFields >> 30u) & 0x3u,
              nativeHull->is_terminal() ? 0u : 1u);
    EXPECT_EQ((serializedLedgeFields >> 28u) & 0x3u,
              nativeHull->is_compact() ? 1u : 0u);
    EXPECT_EQ(serializedLedgeFields & 0x00FFFFFFu,
              static_cast<std::uint32_t>(nativeHull->get_size() / 16));
    EXPECT_EQ(readBigU16(serializedHull + 3 * sizeof(std::uint32_t)),
              static_cast<std::uint16_t>(hullTriangleCount));
    for (int pointIndex = 0; pointIndex < hullPointCount; ++pointIndex) {
        const std::byte *serializedPoint = serializedHull + hullPointOffset +
            pointIndex * sizeof(IVP_Compact_Poly_Point);
        EXPECT_FLOAT_EQ(readBigFloat(serializedPoint),
                        nativeHullPoints[pointIndex].k[0]);
        EXPECT_FLOAT_EQ(readBigFloat(serializedPoint + sizeof(IVP_FLOAT)),
                        nativeHullPoints[pointIndex].k[1]);
        EXPECT_FLOAT_EQ(readBigFloat(serializedPoint + 2 * sizeof(IVP_FLOAT)),
                        nativeHullPoints[pointIndex].k[2]);
    }
    for (int triangleIndex = 0; triangleIndex < hullTriangleCount;
         ++triangleIndex) {
        const IVP_Compact_Triangle *nativeTriangle =
            nativeHull->get_first_triangle() + triangleIndex;
        const std::byte *serializedTriangle =
            serializedHull + sizeof(IVP_Compact_Ledge) +
            triangleIndex * sizeof(IVP_Compact_Triangle);
        const std::uint32_t triangleFields = readBigU32(serializedTriangle);
        EXPECT_EQ((triangleFields >> 20u) & 0xFFFu,
                  static_cast<std::uint32_t>(nativeTriangle->get_tri_index()));
        EXPECT_EQ((triangleFields >> 8u) & 0xFFFu,
                  static_cast<std::uint32_t>(nativeTriangle->get_pierce_index()));
        EXPECT_EQ((triangleFields >> 1u) & 0x7Fu,
                  static_cast<std::uint32_t>(nativeTriangle->get_material_index()));
        EXPECT_EQ(triangleFields & 1u,
                  static_cast<std::uint32_t>(nativeTriangle->get_is_virtual()));
        for (int edgeIndex = 0; edgeIndex < 3; ++edgeIndex) {
            const IVP_Compact_Edge *nativeEdge =
                nativeTriangle->get_edge(edgeIndex);
            const std::uint32_t edgeFields = readBigU32(
                serializedTriangle + sizeof(std::uint32_t) +
                edgeIndex * sizeof(IVP_Compact_Edge));
            EXPECT_EQ((edgeFields >> 16u) & 0xFFFFu,
                      static_cast<std::uint32_t>(
                          nativeEdge->get_start_point_index()));
            std::int32_t opposite =
                static_cast<std::int32_t>((edgeFields >> 1u) & 0x7FFFu);
            if ((opposite & 0x4000) != 0)
                opposite |= ~0x7FFF;
            EXPECT_EQ(opposite, nativeEdge->get_opposite_index());
            EXPECT_EQ(edgeFields & 1u,
                      static_cast<std::uint32_t>(
                          nativeEdge->get_is_virtual()));
        }
    }
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, surface);

    IVP_SurfaceBuilder_Pointsoup::cleanup();
    auto **cachedTriangle = reinterpret_cast<IVP_Compact_Ledge **>(
        gRetailModuleBase + BML::IVP::ABI::PointsoupSingleTriangleRva);
    EXPECT_EQ(*cachedTriangle, nullptr);
}

TEST(IvpRetailPointsoupBuilder,
     CompactMoppHeaderPreservesBallancePackedLayout) {
    alignas(16) std::array<std::byte, 0x40> storage{};
    auto *mopp = reinterpret_cast<IVP_Compact_Mopp *>(storage.data());
    const IVP_FLOAT massCenter[3] = {1.25f, -2.5f, 3.75f};
    const IVP_FLOAT rotationInertia[3] = {4.0f, 5.0f, 6.0f};
    mopp->mass_center.set(massCenter);
    mopp->rotation_inertia.set(rotationInertia);
    mopp->upper_limit_radius = 7.5f;
    mopp->max_factor_surface_deviation = 0x7Fu;
    mopp->byte_size = 0x012345;
    mopp->offset_ledgetree_root = 0x30;
    mopp->offset_ledges = 0x100;
    mopp->size_convex_hull = 0x80;
    mopp->dummy = 0x55667788;

    EXPECT_EQ(mopp->get_size(), 0x012345);
    EXPECT_EQ(reinterpret_cast<const std::byte *>(
                  mopp->get_compact_ledge_tree_root()),
              storage.data() + 0x30);

    mopp->byte_swap();
    const auto readBigU32 = [](const void *address) {
        std::uint32_t stored = 0;
        std::memcpy(&stored, address, sizeof(stored));
        return BML::IVP::Detail::ByteSwap32(stored);
    };
    const auto readBigFloat = [&readBigU32](const void *address) {
        const std::uint32_t bits = readBigU32(address);
        IVP_FLOAT value = 0.0f;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    };
    EXPECT_FLOAT_EQ(readBigFloat(storage.data()), 1.25f);
    EXPECT_FLOAT_EQ(readBigFloat(
                        storage.data() +
                        offsetof(IVP_Compact_Mopp, upper_limit_radius)),
                    7.5f);
    EXPECT_EQ(readBigU32(storage.data() +
                  offsetof(IVP_Compact_Mopp, factor_and_size)),
              0x7F012345u);
    EXPECT_EQ(readBigU32(storage.data() +
                  offsetof(IVP_Compact_Mopp, offset_ledgetree_root)),
              0x30u);
    EXPECT_EQ(readBigU32(storage.data() +
                  offsetof(IVP_Compact_Mopp, offset_ledges)),
              0x100u);
    EXPECT_EQ(readBigU32(storage.data() +
                  offsetof(IVP_Compact_Mopp, size_convex_hull)),
              0x80u);
    EXPECT_EQ(mopp->dummy, 0x55667788);
}

TEST(IvpRetailPointsoupBuilder,
     ExactRetailLedgeRayReturnsStableBooleanAndPayload) {
    static_assert(sizeof(void *) == 4,
                  "The Ballance retail physics ABI is 32-bit x86");
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    // Use an identity object/cache pair so the retained object-space
    // constructor performs its real matrix conversions without requiring an
    // Environment fixture. Static movement prevents a cache refresh.
    alignas(16) std::array<std::byte, sizeof(IVP_Cache_Object)>
        cacheStorage{};
    auto *cache = reinterpret_cast<IVP_Cache_Object *>(cacheStorage.data());
    cache->m_world_f_object.set_identity();
    alignas(16) std::array<std::byte, sizeof(IVP_Real_Object)>
        objectStorage{};
    auto *object = reinterpret_cast<IVP_Real_Object *>(objectStorage.data());
    object->cache_object = cache;
    object->flags = 0;
    object->flags.object_movement_state = IVP_MT_STATIC;

    IVP_U_Point point0(-2.0, -2.0, 0.0);
    IVP_U_Point point1(2.0, -2.0, 0.0);
    IVP_U_Point point2(0.0, 2.0, 0.0);
    IVP_Compact_Ledge *floor =
        IVP_SurfaceBuilder_Pointsoup::convert_triangle_to_compace_ledge(
            &point0, &point1, &point2);
    ASSERT_NE(floor, nullptr);
    ASSERT_EQ(floor->get_n_triangles(), 2);

    IVP_Ray_Solver_Template floorRayTemplate;
    floorRayTemplate.ray_start_point.set(0.0, 0.0, 2.0);
    floorRayTemplate.ray_normized_direction.set(0.0f, 0.0f, -1.0f);
    floorRayTemplate.ray_length = 4.0f;
    IVP_Ray_Solver_Min floorRay(&floorRayTemplate);
    IVP_Ray_Solver_Os floorRayObjectSpace(&floorRay, object);

    EXPECT_EQ(floorRayObjectSpace.check_ray_against_compact_ledge_os(floor),
              IVP_TRUE);
    const IVP_Ray_Hit *floorHit = floorRay.get_ray_hit();
    ASSERT_NE(floorHit, nullptr);
    EXPECT_EQ(floorHit->hit_real_object, object);
    EXPECT_EQ(floorHit->hit_compact_ledge, floor);
    EXPECT_NE(floorHit->hit_compact_triangle, nullptr);
    EXPECT_FLOAT_EQ(floorHit->hit_distance, 2.0f);
    EXPECT_NEAR(floorHit->hit_surface_direction_os.k[0], 0.0f, 1.0e-6f);
    EXPECT_NEAR(floorHit->hit_surface_direction_os.k[1], 0.0f, 1.0e-6f);
    EXPECT_NEAR(floorHit->hit_surface_direction_os.k[2], 1.0f, 1.0e-6f);

    // A second call proves that the temporary forwarding listener was
    // restored rather than leaving a dangling cross-DLL callback target.
    EXPECT_EQ(floorRayObjectSpace.check_ray_against_compact_ledge_os(floor),
              IVP_TRUE);

    IVP_Ray_Solver_Template missRayTemplate = floorRayTemplate;
    missRayTemplate.ray_start_point.set(5.0, 5.0, 2.0);
    IVP_Ray_Solver_Min missRay(&missRayTemplate);
    IVP_Ray_Solver_Os missRayObjectSpace(&missRay, object);
    EXPECT_EQ(missRayObjectSpace.check_ray_against_compact_ledge_os(floor),
              IVP_FALSE);
    EXPECT_EQ(missRay.get_ray_hit(), nullptr);

    // Exercise the general convex-ledge branch as a Ballance obstacle ray,
    // not only the two-sided triangle fast path.
    IVP_U_Point tetra0(0.0, 0.0, 0.0);
    IVP_U_Point tetra1(3.0, 0.0, 0.0);
    IVP_U_Point tetra2(0.0, 4.0, 0.0);
    IVP_U_Point tetra3(0.0, 0.0, 2.0);
    IVP_Vector_of_Points_256 tetrahedron;
    tetrahedron.add(&tetra0);
    tetrahedron.add(&tetra1);
    tetrahedron.add(&tetra2);
    tetrahedron.add(&tetra3);
    IVP_Compact_Ledge *obstacle =
        IVP_SurfaceBuilder_Pointsoup::convert_pointsoup_to_compact_ledge(
            &tetrahedron);
    ASSERT_NE(obstacle, nullptr);
    ASSERT_GT(obstacle->get_n_triangles(), 2);

    IVP_Ray_Solver_Template obstacleRayTemplate;
    obstacleRayTemplate.ray_start_point.set(0.5, 0.5, 3.0);
    obstacleRayTemplate.ray_normized_direction.set(0.0f, 0.0f, -1.0f);
    obstacleRayTemplate.ray_length = 4.0f;
    IVP_Ray_Solver_Min obstacleRay(&obstacleRayTemplate);
    IVP_Ray_Solver_Os obstacleRayObjectSpace(&obstacleRay, object);
    EXPECT_EQ(
        obstacleRayObjectSpace.check_ray_against_compact_ledge_os(obstacle),
        IVP_TRUE);
    const IVP_Ray_Hit *obstacleHit = obstacleRay.get_ray_hit();
    ASSERT_NE(obstacleHit, nullptr);
    EXPECT_EQ(obstacleHit->hit_real_object, object);
    EXPECT_EQ(obstacleHit->hit_compact_ledge, obstacle);
    EXPECT_GT(obstacleHit->hit_distance, 1.0f);
    EXPECT_LT(obstacleHit->hit_distance, 2.0f);
    EXPECT_LT(
        obstacleHit->hit_surface_direction_os.dot_product(
            &obstacleRayTemplate.ray_normized_direction),
        0.0);

    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, obstacle);
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, floor);
    gRetailModuleBase = 0;
}

TEST(IvpRetailPointsoupBuilder,
     ThreeDsCollisionMeshBuildsOriginalBallanceCompactSurface) {
    static_assert(sizeof(void *) == 4,
                  "The Ballance retail physics ABI is 32-bit x86");
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    std::vector<std::byte> bytes;
    const auto append16 = [&bytes](std::uint16_t value) {
        const std::size_t offset = bytes.size();
        bytes.resize(offset + sizeof(value));
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    };
    const auto append32 = [&bytes](std::uint32_t value) {
        const std::size_t offset = bytes.size();
        bytes.resize(offset + sizeof(value));
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    };
    const auto appendFloat = [&bytes](float value) {
        const std::size_t offset = bytes.size();
        bytes.resize(offset + sizeof(value));
        std::memcpy(bytes.data() + offset, &value, sizeof(value));
    };
    const auto beginChunk = [&bytes, &append16, &append32](std::uint16_t id) {
        const std::size_t offset = bytes.size();
        append16(id);
        append32(0);
        return offset;
    };
    const auto endChunk = [&bytes](std::size_t offset) {
        const std::uint32_t length =
            static_cast<std::uint32_t>(bytes.size() - offset);
        std::memcpy(bytes.data() + offset + 2, &length, sizeof(length));
    };

    const std::size_t mainChunk = beginChunk(0x4D4D);
    const std::size_t editChunk = beginChunk(0x3D3D);
    const std::size_t objectChunk = beginChunk(0x4000);
    const char objectName[] = "BallanceRampHull";
    bytes.insert(bytes.end(),
                 reinterpret_cast<const std::byte *>(objectName),
                 reinterpret_cast<const std::byte *>(objectName) +
                     sizeof(objectName));
    const std::size_t meshChunk = beginChunk(0x4100);
    const std::size_t vertexChunk = beginChunk(0x4110);
    append16(4);
    const std::array<std::array<float, 3>, 4> vertices{{
        {{0.0f, 0.0f, 0.0f}}, {{3.0f, 0.0f, 0.0f}},
        {{0.0f, 4.0f, 0.0f}}, {{0.0f, 0.0f, 2.0f}},
    }};
    for (const auto &vertex : vertices) {
        appendFloat(vertex[0]);
        appendFloat(vertex[1]);
        appendFloat(vertex[2]);
    }
    endChunk(vertexChunk);
    const std::size_t faceChunk = beginChunk(0x4120);
    append16(4);
    const std::array<std::array<std::uint16_t, 3>, 4> faces{{
        {{0, 2, 1}}, {{0, 1, 3}}, {{1, 2, 3}}, {{2, 0, 3}},
    }};
    for (const auto &face : faces) {
        append16(face[0]);
        append16(face[1]);
        append16(face[2]);
        append16(0x0007);
    }
    endChunk(faceChunk);
    endChunk(meshChunk);
    endChunk(objectChunk);
    endChunk(editChunk);
    endChunk(mainChunk);

    const std::filesystem::path fixture =
        std::filesystem::temp_directory_path() /
        ("bml-ivp-collision-" + std::to_string(GetCurrentProcessId()) +
         ".3ds");
    {
        std::ofstream output(fixture, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.good());
        output.write(reinterpret_cast<const char *>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        ASSERT_TRUE(output.good());
    }

    IVP_Template_SurfaceBuilder_3ds importParameters;
    importParameters.scale = 2.5f;
    IVP_Concave_Polyhedron *polyhedron =
        IVP_SurfaceBuilder_3ds::convert_3ds_to_concave(
            fixture.string().c_str(), &importParameters);
    const std::uint16_t invalidPointIndex = 9;
    std::memcpy(bytes.data() + faceChunk + 8, &invalidPointIndex,
                sizeof(invalidPointIndex));
    {
        std::ofstream output(fixture, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.good());
        output.write(reinterpret_cast<const char *>(bytes.data()),
                     static_cast<std::streamsize>(bytes.size()));
        ASSERT_TRUE(output.good());
    }
    EXPECT_EQ(IVP_SurfaceBuilder_3ds::convert_3ds_to_concave(
                  fixture.string().c_str(), &importParameters),
              nullptr);
    std::error_code removeError;
    std::filesystem::remove(fixture, removeError);
    ASSERT_FALSE(removeError);
    ASSERT_NE(polyhedron, nullptr);
    ASSERT_EQ(polyhedron->points.len(), 4);
    ASSERT_EQ(polyhedron->faces.len(), 4);
    EXPECT_DOUBLE_EQ(polyhedron->points.element_at(1)->k[0], 7.5);
    EXPECT_DOUBLE_EQ(polyhedron->points.element_at(2)->k[1], 10.0);
    EXPECT_DOUBLE_EQ(polyhedron->points.element_at(3)->k[2], 5.0);

    IVP_Convex_Decompositor_Parameters decompositionParameters;
    decompositionParameters.tolin = 1.0e-6f;
    IVP_Compact_Surface *surface =
        IVP_SurfaceBuilder_Polyhedron_Concave::
            convert_concave_polyhedron_to_single_compact_surface(
                polyhedron, &decompositionParameters);
    ASSERT_NE(surface, nullptr);
    IVP_SurfaceManager_Polygon manager(surface);
    IVP_U_BigVector<IVP_Compact_Ledge> ledges;
    manager.get_all_terminal_ledges(&ledges);
    ASSERT_EQ(ledges.len(), 1);
    EXPECT_EQ(ledges.element_at(0)->get_n_points(), 4);
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, surface);

    for (int index = polyhedron->faces.len() - 1; index >= 0; --index)
        delete polyhedron->faces.element_at(index);
    for (int index = polyhedron->points.len() - 1; index >= 0; --index)
        delete polyhedron->points.element_at(index);
    delete polyhedron;
    gRetailModuleBase = 0;
}

TEST(IvpRetailPointsoupBuilder,
     QuakeBspBrushBuildsOriginalBallanceCompactGeometry) {
    static_assert(sizeof(void *) == 4,
                  "The Ballance retail physics ABI is 32-bit x86");
    RetailImage physics;
    if (!physics.configured()) {
        GTEST_SKIP() << "Set BML_TEST_RETAIL_PHYSICS_DLL to the exact "
                        "Ballance BuildingBlocks\\physics_RT.dll";
    }
    ASSERT_TRUE(physics.valid()) << physics.error();

    dmodel_t model{};
    model.mins[0] = model.mins[1] = model.mins[2] = -2.0f;
    model.maxs[0] = model.maxs[1] = model.maxs[2] = 2.0f;
    model.headnode[0] = 0;
    std::array<dplane_t, 6> planes{};
    const std::array<std::array<float, 3>, 6> normals{{
        {{1.0f, 0.0f, 0.0f}}, {{-1.0f, 0.0f, 0.0f}},
        {{0.0f, 1.0f, 0.0f}}, {{0.0f, -1.0f, 0.0f}},
        {{0.0f, 0.0f, 1.0f}}, {{0.0f, 0.0f, -1.0f}},
    }};
    std::array<dnode_t, 6> nodes{};
    for (int index = 0; index < 6; ++index) {
        std::copy(normals[index].begin(), normals[index].end(),
                  planes[index].normal);
        planes[index].dist = -2.0f;
        nodes[index].planenum = index;
        nodes[index].children[0] =
            static_cast<short>(index + 1 < 6 ? index + 1 : -1);
        nodes[index].children[1] = -2;
    }

    IVP_SurfaceBuilder_Q12 memoryBuilder;
    memoryBuilder.init_q12bsp_from_memory(
        30, 1, &model, static_cast<int>(planes.size()), planes.data(),
        static_cast<int>(nodes.size()), nodes.data(), 0, nullptr);
    IVP_U_Vector<IVP_Compact_Ledge> ledges;
    memoryBuilder.convert_q12bsp_model_to_compact_ledges(
        0, 0.5, 0.1, 0.001f, &ledges);
    ASSERT_EQ(ledges.len(), 1);
    const IVP_Compact_Ledge *brush = ledges.element_at(0);
    ASSERT_EQ(brush->get_n_points(), 8);
    IVP_U_Point minimum(100.0, 100.0, 100.0);
    IVP_U_Point maximum(-100.0, -100.0, -100.0);
    for (int pointIndex = 0; pointIndex < brush->get_n_points(); ++pointIndex) {
        const IVP_Compact_Poly_Point &point =
            brush->get_point_array()[pointIndex];
        for (int axis = 0; axis < 3; ++axis) {
            minimum.k[axis] = (std::min)(minimum.k[axis],
                                         static_cast<double>(point.k[axis]));
            maximum.k[axis] = (std::max)(maximum.k[axis],
                                         static_cast<double>(point.k[axis]));
        }
    }
    for (int axis = 0; axis < 3; ++axis) {
        EXPECT_NEAR(minimum.k[axis], -0.9, 1.0e-5);
        EXPECT_NEAR(maximum.k[axis], 0.9, 1.0e-5);
    }
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, ledges.element_at(0));
    memoryBuilder.unload_q12bsp();
    IVP_U_Vector<IVP_Compact_Ledge> afterUnload;
    memoryBuilder.convert_q12bsp_model_to_compact_ledges(
        0, 1.0, 0.0, 0.001f, &afterUnload);
    EXPECT_EQ(afterUnload.len(), 0);

    std::vector<std::byte> bspBytes(sizeof(dheader_t));
    const auto appendArray = [&bspBytes](const void *data, std::size_t size) {
        const std::size_t offset = bspBytes.size();
        bspBytes.resize(offset + size);
        std::memcpy(bspBytes.data() + offset, data, size);
        return static_cast<int>(offset);
    };
    dheader_t header{};
    header.version = 30;
    header.lumps[14].fileofs = appendArray(&model, sizeof(model));
    header.lumps[14].filelen = sizeof(model);
    header.lumps[1].fileofs =
        appendArray(planes.data(), sizeof(planes));
    header.lumps[1].filelen = sizeof(planes);
    header.lumps[5].fileofs = appendArray(nodes.data(), sizeof(nodes));
    header.lumps[5].filelen = sizeof(nodes);
    header.lumps[9].fileofs = static_cast<int>(bspBytes.size());
    header.lumps[9].filelen = 0;
    std::memcpy(bspBytes.data(), &header, sizeof(header));

    const std::filesystem::path fixture =
        std::filesystem::temp_directory_path() /
        ("bml-ivp-q12-brush-" + std::to_string(GetCurrentProcessId()) +
         ".bsp");
    {
        std::ofstream output(fixture, std::ios::binary | std::ios::trunc);
        ASSERT_TRUE(output.good());
        output.write(reinterpret_cast<const char *>(bspBytes.data()),
                     static_cast<std::streamsize>(bspBytes.size()));
        ASSERT_TRUE(output.good());
    }
    IVP_SurfaceBuilder_Q12 diskBuilder;
    std::string fixtureName = fixture.string();
    ASSERT_EQ(diskBuilder.load_q12bsp_file(fixtureName.data()), 1);
    IVP_Compact_Surface *surface =
        diskBuilder.convert_q12bsp_model_to_single_compact_surface(
            0, 1.0, 0.0, 0.001f);
    ASSERT_NE(surface, nullptr);
    IVP_SurfaceManager_Polygon manager(surface);
    IVP_U_BigVector<IVP_Compact_Ledge> diskLedges;
    manager.get_all_terminal_ledges(&diskLedges);
    ASSERT_EQ(diskLedges.len(), 1);
    EXPECT_EQ(diskLedges.element_at(0)->get_n_points(), 8);
    BML::IVP::ABI::Invoke<void>(
        BML::IVP::ABI::Address::FreeAligned, surface);
    diskBuilder.unload_q12bsp();

    std::error_code removeError;
    std::filesystem::remove(fixture, removeError);
    ASSERT_FALSE(removeError);
    gRetailModuleBase = 0;
}
