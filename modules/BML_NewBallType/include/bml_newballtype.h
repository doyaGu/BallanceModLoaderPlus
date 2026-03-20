#ifndef BML_NEWBALLTYPE_H
#define BML_NEWBALLTYPE_H

#include "bml_errors.h"
#include "bml_interface.h"
#include "bml_types.h"

BML_BEGIN_CDECLS

typedef struct BML_NewBallTypeBallDesc {
    size_t struct_size;
    const char *ball_file_utf8;
    const char *ball_id_utf8;
    const char *ball_name_utf8;
    const char *object_name_utf8;
    float friction;
    float elasticity;
    float mass;
    const char *collision_group_utf8;
    float linear_damp;
    float rotational_damp;
    float force;
    float radius;
} BML_NewBallTypeBallDesc;

typedef struct BML_NewBallTypeFloorDesc {
    size_t struct_size;
    const char *name_utf8;
    float friction;
    float elasticity;
    float mass;
    const char *collision_group_utf8;
    int enable_collision;
} BML_NewBallTypeFloorDesc;

typedef struct BML_NewBallTypeModuleConvexDesc {
    size_t struct_size;
    const char *name_utf8;
    int fixed;
    float friction;
    float elasticity;
    float mass;
    const char *collision_group_utf8;
    int frozen;
    int enable_collision;
    int calc_mass_center;
    float linear_damp;
    float rotational_damp;
} BML_NewBallTypeModuleConvexDesc;

typedef struct BML_NewBallTypeModuleBallDesc {
    size_t struct_size;
    const char *name_utf8;
    int fixed;
    float friction;
    float elasticity;
    float mass;
    const char *collision_group_utf8;
    int frozen;
    int enable_collision;
    int calc_mass_center;
    float linear_damp;
    float rotational_damp;
    float radius;
} BML_NewBallTypeModuleBallDesc;

typedef struct BML_NewBallTypeInterface {
    BML_InterfaceHeader header;
    BML_Result (*RegisterBallType)(const BML_NewBallTypeBallDesc *desc);
    BML_Result (*RegisterFloorType)(const BML_NewBallTypeFloorDesc *desc);
    BML_Result (*RegisterModuleBall)(const BML_NewBallTypeModuleBallDesc *desc);
    BML_Result (*RegisterModuleConvex)(const BML_NewBallTypeModuleConvexDesc *desc);
    BML_Result (*RegisterTrafo)(const char *name_utf8);
    BML_Result (*RegisterModule)(const char *name_utf8);
} BML_NewBallTypeInterface;

#define BML_NEWBALLTYPE_BALL_DESC_INIT { sizeof(BML_NewBallTypeBallDesc), NULL, NULL, NULL, NULL, 0.7f, 0.4f, 1.0f, NULL, 0.1f, 0.1f, 0.0f, 0.0f }
#define BML_NEWBALLTYPE_FLOOR_DESC_INIT { sizeof(BML_NewBallTypeFloorDesc), NULL, 0.7f, 0.4f, 1.0f, NULL, 1 }
#define BML_NEWBALLTYPE_MODULE_CONVEX_DESC_INIT { sizeof(BML_NewBallTypeModuleConvexDesc), NULL, 0, 0.7f, 0.4f, 1.0f, NULL, 0, 1, 0, 0.1f, 0.1f }
#define BML_NEWBALLTYPE_MODULE_BALL_DESC_INIT { sizeof(BML_NewBallTypeModuleBallDesc), NULL, 0, 0.7f, 0.4f, 1.0f, NULL, 0, 1, 0, 0.1f, 0.1f, 0.0f }

#define BML_NEWBALLTYPE_INTERFACE_ID "bml.newballtype"

BML_END_CDECLS

#endif /* BML_NEWBALLTYPE_H */
