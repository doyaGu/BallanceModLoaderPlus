#ifndef BML_VIRTOOLS_PAYLOADS_H
#define BML_VIRTOOLS_PAYLOADS_H

#include "CKTypes.h"

class CK3dEntity;
class CKBehavior;
class CKMesh;
class CKObject;

typedef struct BML_LegacyObjectLoadPayload {
    size_t struct_size;
    const char *filename;
    const char *master_name;
    CK_CLASSID class_id;
    CKBOOL add_to_scene;
    CKBOOL reuse_meshes;
    CKBOOL reuse_materials;
    CKBOOL dynamic;
    CKObject *master_object;
    CKBOOL is_map;
    CK_ID *object_ids;
    uint32_t object_count;
} BML_LegacyObjectLoadPayload;

typedef struct BML_LegacyScriptLoadPayload {
    size_t struct_size;
    const char *filename;
    CKBehavior *script;
    CKBOOL is_map;
} BML_LegacyScriptLoadPayload;

typedef struct BML_LegacyPhysicalizePayload {
    size_t struct_size;
    CK3dEntity *target;
    CKBOOL fixed;
    float friction;
    float elasticity;
    float mass;
    const char *collision_group;
    CKBOOL start_frozen;
    CKBOOL enable_collision;
    CKBOOL auto_calc_mass_center;
    float linear_speed_dampening;
    float rot_speed_dampening;
    const char *collision_surface;
    VxVector mass_center;
    int convex_count;
    CKMesh **convex_meshes;
    int ball_count;
    VxVector *ball_centers;
    float *ball_radii;
    int concave_count;
    CKMesh **concave_meshes;
} BML_LegacyPhysicalizePayload;

#endif // BML_VIRTOOLS_PAYLOADS_H