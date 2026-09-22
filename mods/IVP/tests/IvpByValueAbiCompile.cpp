#include "BML/IVP/IVP.h"

#include <cstdint>
#include <type_traits>

template <typename Enum>
inline constexpr bool kIvpRetailEnum =
    std::is_enum_v<Enum> && sizeof(Enum) == sizeof(std::int32_t) &&
    std::is_same_v<std::underlying_type_t<Enum>, std::int32_t>;

static_assert(sizeof(IVP_FLOAT) == 4);
static_assert(sizeof(IVP_DOUBLE) == 8);
static_assert(sizeof(IVP_Time) == 8);
static_assert(alignof(IVP_Time) == 8);
static_assert(std::is_trivially_copyable_v<IVP_Time>);
static_assert(std::is_trivially_destructible_v<IVP_Time>);

static_assert(kIvpRetailEnum<IVP_ACTUATOR_TYPE>);
static_assert(kIvpRetailEnum<IVP_SPRING_FORCE_EXCEED>);
static_assert(kIvpRetailEnum<IVP_POS_WHEEL>);
static_assert(kIvpRetailEnum<IVP_POS_AXIS>);
static_assert(kIvpRetailEnum<IVP_CONSTRAINT_AXIS_TYPE>);
static_assert(kIvpRetailEnum<IVP_CONSTRAINT_FORCE_EXCEED>);
static_assert(kIvpRetailEnum<IVP_CONSTRAINT_FLAGS>);
static_assert(kIvpRetailEnum<IVP_TRANSROT_INDEX>);
static_assert(kIvpRetailEnum<IVP_NORM>);
static_assert(kIvpRetailEnum<IVP_HULL_ELEM_TYPE>);
static_assert(kIvpRetailEnum<IVP_CONTROLLER_PRIORITY>);
static_assert(kIvpRetailEnum<IVP_GOLEM_PROBLEM>);
static_assert(kIvpRetailEnum<IVP_DEBUG_CLASS>);
static_assert(kIvpRetailEnum<IVP_U_INTERSECT_TYPE>);
static_assert(kIvpRetailEnum<IVP_CONTACT_POINT_BREAK_STATUS>);
static_assert(kIvpRetailEnum<IVP_OBJECT_TYPE>);
static_assert(kIvpRetailEnum<IVP_SYNAPSE_POLYGON_STATUS>);
static_assert(kIvpRetailEnum<IVP_MINIMAL_DIST_STATUS>);
static_assert(kIvpRetailEnum<IVP_MINIMAL_DIST_RECALC_RESULT>);
static_assert(kIvpRetailEnum<IVP_COLL_TYPE>);
static_assert(kIvpRetailEnum<IVP_MINDIST_FUNCTION>);
static_assert(kIvpRetailEnum<IVP_MRC_TYPE>);
static_assert(kIvpRetailEnum<IVP_MINDIST_EVENT_HINT>);
static_assert(kIvpRetailEnum<IVP_BETTERSTATISTICSMANAGER_DATA_ENTITY_TYPE>);
static_assert(kIvpRetailEnum<IVP_PERFORMANCE_ELEMENT>);
static_assert(kIvpRetailEnum<IVP_2P_RET>);
static_assert(kIvpRetailEnum<IVP_RAY_SOLVER_FLAGS>);
static_assert(kIvpRetailEnum<IVP_LISTENER_COLLISION_CALLBACKS>);
static_assert(kIvpRetailEnum<IVP_BOOL>);
static_assert(kIvpRetailEnum<IVP_RETURN_TYPE>);
static_assert(kIvpRetailEnum<IVP_COORDINATE_INDEX>);
static_assert(kIvpRetailEnum<IVP_Movement_Type>);
static_assert(kIvpRetailEnum<IVP_ENV_STATE>);
static_assert(kIvpRetailEnum<IVP_MATRIX_CATEGORY>);
static_assert(kIvpRetailEnum<IVP_SURBUILD_LEDGE_SOUP_MERGE_POINT_TYPES>);
static_assert(kIvpRetailEnum<IVP_SURMAN_TYPE>);

static_assert(std::is_same_v<decltype(&IVP_Controller::reset_time),
                             void (IVP_Controller::*)(IVP_Time)>);
static_assert(std::is_same_v<decltype(&IVP_Core::calc_at_matrix),
                             void (IVP_Core::*)(IVP_Time,
                                                IVP_U_Matrix *) const>);
static_assert(std::is_same_v<decltype(&IVP_Core::calc_movement_state),
                             IVP_Movement_Type (IVP_Core::*)(IVP_Time)>);
static_assert(std::is_same_v<
              decltype(&IVP_PerformanceCounter_Simple::
                           reset_and_print_performance_counters),
              void (IVP_PerformanceCounter_Simple::*)(IVP_Time)>);
static_assert(std::is_same_v<decltype(&IVP_Real_Object::calc_at_matrix),
                             void (IVP_Real_Object::*)(IVP_Time,
                                                       IVP_U_Matrix *) const>);

using EnvironmentTimeGetter = IVP_Time (IVP_Environment::*)();
using ConstEnvironmentTimeGetter = IVP_Time (IVP_Environment::*)() const;
static_assert(std::is_same_v<
              decltype(static_cast<EnvironmentTimeGetter>(
                  &IVP_Environment::get_current_time)),
              EnvironmentTimeGetter>);
static_assert(std::is_same_v<
              decltype(static_cast<ConstEnvironmentTimeGetter>(
                  &IVP_Environment::get_current_time)),
              ConstEnvironmentTimeGetter>);
static_assert(std::is_same_v<decltype(&IVP_Time::operator+),
                             IVP_Time (IVP_Time::*)(double) const>);
static_assert(std::is_same_v<
              decltype(&IVP_Controller_World_Friction::
                           set_friction_value_translation),
              void (IVP_Controller_World_Friction::*)(IVP_U_Point)>);
static_assert(std::is_same_v<
              decltype(&IVP_Controller_World_Friction::
                           set_friction_value_rotation),
              void (IVP_Controller_World_Friction::*)(IVP_U_Point)>);
