#ifndef BML_VIRTOOLS_PAYLOADS_H
#define BML_VIRTOOLS_PAYLOADS_H

#include "CKTypes.h"

class CK3dEntity;
class CKBehavior;
class CKMesh;
class CKObject;

/**
 * @brief Payload for BML/ObjectLoad/LoadObject events.
 *
 * All pointer members borrow from the CK engine and are valid only
 * during the frame in which the event is published.
 */
typedef struct BML_ObjectLoadEvent {
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
    const CK_ID *object_ids;   /**< Borrowed; valid for this frame only */
    uint32_t object_count;
} BML_ObjectLoadEvent;

#define BML_OBJECT_LOAD_EVENT_INIT { sizeof(BML_ObjectLoadEvent) }

/**
 * @brief Payload for BML/ObjectLoad/LoadScript events.
 */
typedef struct BML_ScriptLoadEvent {
    size_t struct_size;
    const char *filename;
    CKBehavior *script;
    CKBOOL is_map;
} BML_ScriptLoadEvent;

#define BML_SCRIPT_LOAD_EVENT_INIT { sizeof(BML_ScriptLoadEvent) }

/**
 * @brief Payload for Physics/Physicalize events.
 *
 * All pointer members borrow from the CK engine and are valid only
 * during the frame in which the event is published.
 */
typedef struct BML_PhysicalizeEvent {
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
    CKMesh **convex_meshes;     /**< Borrowed; valid for this frame only */
    int ball_count;
    VxVector *ball_centers;     /**< Borrowed; valid for this frame only */
    float *ball_radii;          /**< Borrowed; valid for this frame only */
    int concave_count;
    CKMesh **concave_meshes;    /**< Borrowed; valid for this frame only */
} BML_PhysicalizeEvent;

/**
 * @brief Payload for Physics/PostProcess events.
 *
 * Published after each IpionManager physics simulation step.
 */
typedef struct BML_PhysicsStepEvent {
    float delta_time;       /**< Physics simulation delta time */
} BML_PhysicsStepEvent;

/* ========================================================================
 * Game Command Payloads
 * ======================================================================== */

class CKBeObject;

/**
 * @brief Payload for BML/Command/SetIC.
 * Sets Initial Conditions on a CKBeObject.
 */
typedef struct BML_SetICCommand {
    CKBeObject *target;     /**< Object to set IC on. */
    CKBOOL hierarchy;       /**< Apply recursively to children. */
} BML_SetICCommand;

/**
 * @brief Payload for BML/Command/RestoreIC.
 * Restores Initial Conditions on a CKBeObject.
 */
typedef struct BML_RestoreICCommand {
    CKBeObject *target;     /**< Object to restore IC on. */
    CKBOOL hierarchy;       /**< Apply recursively to children. */
} BML_RestoreICCommand;

/**
 * @brief Payload for BML/Command/Show.
 * Shows or hides a CKBeObject.
 *
 * show_option values (CK_OBJECT_SHOWOPTION):
 *   CKSHOW  (1) = show,
 *   CKHIDE  (0) = hide,
 *   CKHIERARCHICALHIDE (-1) = hide with hierarchy flag
 */
typedef struct BML_ShowCommand {
    CKBeObject *target;     /**< Object to show/hide. */
    int show_option;        /**< CK_OBJECT_SHOWOPTION value. */
    CKBOOL hierarchy;       /**< Apply recursively to children. */
} BML_ShowCommand;

#endif // BML_VIRTOOLS_PAYLOADS_H
