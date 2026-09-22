#ifndef BML_IVP_CONSTRAINT_CAR_H
#define BML_IVP_CONSTRAINT_CAR_H

#include "BML/IVP/Constraint.h"
#include "BML/IVP/GreatMatrix.h"

#include <cmath>
#include <cstddef>

#ifndef IVP_CONSTRAINT_CAR_MAX_WHEELS
#define IVP_CONSTRAINT_CAR_MAX_WHEELS 12
#endif

class IVP_Constraint_Solver_Car;

// One real wheel (or the body at index zero) in the early constraint-based
// vehicle solver. Its fields agree exactly between the imported Ballance UDT
// and the neighboring public header; no concrete retail body survives.
class IVP_Constraint_Car_Object {
public:
    IVP_Constraint_Car_Object(
        IVP_Constraint_Solver_Car *solverCar,
        IVP_Real_Object *appendedObject,
        IVP_Real_Object *bodyObject,
        IVP_U_Float_Point *targetBodyOverride = nullptr)
        : real_object(appendedObject), solver_car(solverCar),
          last_skid_value(0.0f), last_skid_time(0.0),
          fix_wheel_constraint(nullptr) {
        real_object->get_core()->car_wheel = this;
        last_contact_position_ws.set_to_zero();

        if (!bodyObject) {
            target_position_bs.set_identity();
            return;
        }

        if (targetBodyOverride) {
            target_position_bs.set_identity();
            IVP_U_Matrix coreFromObject;
            bodyObject->calc_m_core_f_object(&coreFromObject);
            coreFromObject.inline_vmult4(
                targetBodyOverride, &target_position_bs.vv);
            return;
        }

        bodyObject->get_core()->get_m_world_f_core_PSI()->inline_mimult4(
            appendedObject->get_core()->get_m_world_f_core_PSI(),
            &target_position_bs);
    }

    ~IVP_Constraint_Car_Object() {
        if (real_object && real_object->get_core()->car_wheel == this)
            real_object->get_core()->car_wheel = nullptr;
    }

    IVP_Core *get_core() const { return real_object->get_core(); }

    IVP_Real_Object *real_object;
    IVP_Constraint_Solver_Car *solver_car;
    IVP_U_Matrix target_position_bs;
    IVP_U_Float_Point last_contact_position_ws;
    IVP_FLOAT last_skid_value;
    IVP_Time last_skid_time;
    IVP_Constraint *fix_wheel_constraint;
};

class IVP_Constraint_Solver_Car_Builder;

// Fast bilateral x/z real-wheel constraint solver. This is a selective source
// reconstruction over Ballance-confirmed Core, matrix and controller ABI; it
// adds no virtual slots beyond the seven-slot IVP_Controller contract.
class IVP_Constraint_Solver_Car : public IVP_Controller_Dependent {
    friend class IVP_Constraint_Solver_Car_Builder;

public:
    IVP_Constraint_Solver_Car(
        IVP_COORDINATE_INDEX right, IVP_COORDINATE_INDEX up,
        IVP_COORDINATE_INDEX forward, IVP_BOOL isLeftHanded)
        : body_object(nullptr), x_idx(right), y_idx(up), z_idx(forward),
          angle_sign(isLeftHanded ? -1.0f : 1.0f),
          local_translation_in_use(IVP_FALSE), environment(nullptr),
          psis_left_for_plan_B(0), max_delta_speed(240.0f) {
        for (int index = 0; index < 6; ++index)
            constraint_is_disabled[index] = IVP_FALSE;
        for (int index = 0; index < IVP_CONSTRAINT_CAR_MAX_WHEELS;
             ++index) {
            c_local_ballsocket[index] = nullptr;
        }
    }

    ~IVP_Constraint_Solver_Car() override {
        if (environment) {
            IVP_Controller_Manager::remove_controller_from_environment(
                this, IVP_TRUE);
        }

        delete body_object;
        body_object = nullptr;
        for (int index = 0; index < wheel_objects.len(); ++index) {
            delete wheel_objects.element_at(index);
            if (c_local_ballsocket[index])
                delete c_local_ballsocket[index];
            c_local_ballsocket[index] = nullptr;
        }
        wheel_objects.remove_all();

        release_matrix_storage(co_matrix);
    }

    IVP_RETURN_TYPE init_constraint_system(
        IVP_Environment *targetEnvironment, IVP_Real_Object *body,
        IVP_U_Vector<IVP_Real_Object> &wheels,
        IVP_U_Vector<IVP_U_Float_Point> &positionsBodySpace);

    void do_simulation_controller(
        IVP_Event_Sim *event,
        IVP_U_Vector<IVP_Core> *coreList) override;

    IVP_U_Vector<IVP_Core> *get_associated_controlled_cores() override {
        return &cores_of_constraint_system;
    }
    int get_num_of_appending_terminals() { return wheel_objects.len(); }

    IVP_Constraint_Car_Object *body_object;
    IVP_U_Vector<IVP_Constraint_Car_Object> wheel_objects;
    IVP_Great_Matrix_Many_Zero co_matrix;
    int x_idx;
    int y_idx;
    int z_idx;
    IVP_FLOAT angle_sign;

protected:
    void core_is_going_to_be_deleted_event(IVP_Core *) override {
        delete this;
    }
    IVP_DOUBLE get_minimum_simulation_frequency() override { return 30.0; }
    IVP_CONTROLLER_PRIORITY get_controller_priority() override {
        return IVP_CP_CONSTRAINTS;
    }

    void do_simulation_controller_rotation(
        IVP_Event_Sim *event, IVP_Core *bodyCore,
        const IVP_U_Matrix *worldFromBody);

    IVP_U_Vector<IVP_Core> cores_of_constraint_system;
    IVP_BOOL constraint_is_disabled[6];
    IVP_BOOL local_translation_in_use;
    IVP_Constraint *c_local_ballsocket[IVP_CONSTRAINT_CAR_MAX_WHEELS];
    IVP_Environment *environment;
    int psis_left_for_plan_B;
    IVP_FLOAT max_delta_speed;

private:
    static void release_matrix_storage(
        IVP_Great_Matrix_Many_Zero &matrix) {
        if (matrix.matrix_values) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::Free, matrix.matrix_values);
        }
        if (matrix.result_vector) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::Free, matrix.result_vector);
        }
        if (matrix.desired_vector) {
            BML::IVP::ABI::Invoke<void>(
                BML::IVP::ABI::Address::Free, matrix.desired_vector);
        }
        matrix.matrix_values = nullptr;
        matrix.result_vector = nullptr;
        matrix.desired_vector = nullptr;
        matrix.columns = 0;
        matrix.aligned_row_len = 0;
    }
};

class IVP_Constraint_Solver_Car_Builder {
public:
    explicit IVP_Constraint_Solver_Car_Builder(
        IVP_Constraint_Solver_Car *solver)
        : car_solver(solver), n_appends(solver->get_num_of_appending_terminals()),
          n_constraints(6) {
        for (int index = 0; index < 6; ++index)
            car_solver->constraint_is_disabled[index] = IVP_FALSE;
    }

    void disable_constraint(int constraint) {
        if (constraint < 0 || constraint >= 6)
            return;
        if (car_solver->constraint_is_disabled[constraint] == IVP_FALSE) {
            car_solver->constraint_is_disabled[constraint] = IVP_TRUE;
            --n_constraints;
        }
    }

    IVP_RETURN_TYPE calc_constraint_matrix();

    IVP_Constraint_Solver_Car *car_solver;
    int n_appends;
    int n_constraints;

private:
    void calc_pushing_behavior(int appendedObject, int pushVector);

    IVP_Great_Matrix_Many_Zero tmp_matrix;
};

inline IVP_RETURN_TYPE IVP_Constraint_Solver_Car::init_constraint_system(
    IVP_Environment *targetEnvironment, IVP_Real_Object *body,
    IVP_U_Vector<IVP_Real_Object> &wheels,
    IVP_U_Vector<IVP_U_Float_Point> &positionsBodySpace) {
    if (!targetEnvironment || !body || wheels.len() <= 0 ||
        wheels.len() > IVP_CONSTRAINT_CAR_MAX_WHEELS ||
        positionsBodySpace.len() < wheels.len() || body_object) {
        return IVP_FAULT;
    }

    environment = targetEnvironment;
    body_object = new IVP_Constraint_Car_Object(this, body, nullptr, nullptr);
    cores_of_constraint_system.add(body->get_core());
    for (int index = 0; index < wheels.len(); ++index) {
        IVP_Real_Object *wheel = wheels.element_at(index);
        wheel_objects.add(new IVP_Constraint_Car_Object(
            this, wheel, body, positionsBodySpace.element_at(index)));
        cores_of_constraint_system.add(wheel->get_core());
        c_local_ballsocket[index] = nullptr;
    }

    targetEnvironment->get_controller_manager()
        ->announce_controller_to_environment(this);

    IVP_Constraint_Solver_Car_Builder builder(this);
    builder.disable_constraint(y_idx);
    builder.disable_constraint(x_idx + 3);
    builder.disable_constraint(y_idx + 3);
    builder.disable_constraint(z_idx + 3);
    return builder.calc_constraint_matrix();
}

inline void IVP_Constraint_Solver_Car::do_simulation_controller_rotation(
    IVP_Event_Sim *event, IVP_Core *bodyCore,
    const IVP_U_Matrix *worldFromBody) {
    for (int index = 0; index < wheel_objects.len(); ++index) {
        IVP_Constraint_Car_Object *wheel = wheel_objects.element_at(index);
        IVP_Core *wheelCore = wheel->get_core();
        const IVP_U_Matrix *worldFromWheel =
            wheelCore->get_m_world_f_core_PSI();

        IVP_U_Point targetAxisBody;
        IVP_U_Point targetAxisWorld;
        IVP_U_Point targetAxisWheel;
        wheel->target_position_bs.get_col(
            static_cast<IVP_COORDINATE_INDEX>(x_idx), &targetAxisBody);
        worldFromBody->inline_vmult3(&targetAxisBody, &targetAxisWorld);
        worldFromWheel->inline_vimult3(&targetAxisWorld, &targetAxisWheel);

        const IVP_DOUBLE alpha =
            wheelCore->rot_speed.k[x_idx] * event->delta_time * 0.45;
        const IVP_DOUBLE sine =
            angle_sign * IVP_Inline_Math::approx5_sin(
                             static_cast<IVP_FLOAT>(alpha));
        const IVP_DOUBLE cosine = IVP_Inline_Math::approx5_cos(
            static_cast<IVP_FLOAT>(alpha));
        const IVP_DOUBLE nextZ =
            targetAxisWheel.k[z_idx] * cosine -
            targetAxisWheel.k[y_idx] * sine;
        const IVP_DOUBLE nextY =
            targetAxisWheel.k[y_idx] * cosine +
            targetAxisWheel.k[z_idx] * sine;
        const IVP_DOUBLE angle0 = angle_sign *
            -IVP_Inline_Math::atan2d(
                nextZ, targetAxisWheel.k[x_idx]);
        const IVP_DOUBLE angle1 = angle_sign *
            IVP_Inline_Math::atan2d(
                nextY, targetAxisWheel.k[x_idx]);

        IVP_U_Float_Point rotationAxisY;
        IVP_U_Float_Point rotationAxisZ;
        worldFromWheel->get_col(
            static_cast<IVP_COORDINATE_INDEX>(y_idx), &rotationAxisY);
        worldFromWheel->get_col(
            static_cast<IVP_COORDINATE_INDEX>(z_idx), &rotationAxisZ);

        IVP_Solver_Core_Reaction reaction;
        reaction.init_reaction_solver_rotation_ws(
            bodyCore, wheelCore, &rotationAxisY, &rotationAxisZ, nullptr);
        IVP_U_Matrix3 &response =
            reaction.m_velocity_ds_f_impulse_ds;
        IVP_DOUBLE inverse00;
        IVP_DOUBLE inverse01;
        IVP_DOUBLE inverse10;
        IVP_DOUBLE inverse11;
        if (IVP_Inline_Math::invert_2x2_matrix(
                response.get_elem(0, 0), response.get_elem(0, 1),
                response.get_elem(0, 1), response.get_elem(1, 1),
                &inverse00, &inverse01, &inverse10, &inverse11) ==
            IVP_FAULT) {
            continue;
        }

        const IVP_DOUBLE desired0 =
            -event->i_delta_time * angle0 -
            reaction.delta_velocity_ds.k[0];
        const IVP_DOUBLE desired1 =
            -event->i_delta_time * angle1 -
            reaction.delta_velocity_ds.k[1];
        IVP_U_Float_Point impulse;
        impulse.set(
            static_cast<IVP_FLOAT>(
                desired0 * inverse00 + desired1 * inverse01),
            static_cast<IVP_FLOAT>(
                desired0 * inverse10 + desired1 * inverse11),
            0.0f);
        reaction.exert_angular_impulse_dim2(
            bodyCore, wheelCore, impulse);
    }
}

inline void IVP_Constraint_Solver_Car::do_simulation_controller(
    IVP_Event_Sim *event, IVP_U_Vector<IVP_Core> *) {
    if (!event || !body_object || !co_matrix.desired_vector ||
        !co_matrix.result_vector) {
        return;
    }

    IVP_Core *bodyCore = body_object->get_core();
    const IVP_U_Matrix *worldFromBody =
        bodyCore->get_m_world_f_core_PSI();
    do_simulation_controller_rotation(event, bodyCore, worldFromBody);

    IVP_DOUBLE *input = co_matrix.desired_vector;
    bool startLocalTranslation = false;
    int invalidCount = 0;
    for (int index = 0; index < wheel_objects.len(); ++index) {
        IVP_Constraint_Car_Object *wheel = wheel_objects.element_at(index);
        IVP_Core *wheelCore = wheel->get_core();
        const IVP_U_Matrix *worldFromWheel =
            wheelCore->get_m_world_f_core_PSI();

        IVP_U_Float_Point currentWheelBody;
        IVP_U_Float_Point targetBody(
            wheel->target_position_bs.get_position());
        worldFromBody->inline_vimult4(
            &worldFromWheel->vv, &currentWheelBody);

        IVP_U_Float_Point wheelSpeedBody;
        IVP_U_Float_Point bodySurfaceWorld;
        IVP_U_Float_Point bodySurfaceBody;
        worldFromBody->inline_vimult3(
            &wheelCore->speed, &wheelSpeedBody);
        bodyCore->get_surface_speed(&targetBody, &bodySurfaceWorld);
        worldFromBody->inline_vimult3(
            &bodySurfaceWorld, &bodySurfaceBody);

        IVP_U_Float_Point nextWheelBody;
        IVP_U_Float_Point nextTargetBody;
        nextWheelBody.add_multiple(
            &currentWheelBody, &wheelSpeedBody, event->delta_time);
        nextTargetBody.add_multiple(
            &targetBody, &bodySurfaceBody, event->delta_time);
        IVP_U_Float_Point requiredDelta;
        requiredDelta.subtract(&nextWheelBody, &nextTargetBody);
        requiredDelta.mult(event->i_delta_time);

        if (requiredDelta.quad_length() >
            max_delta_speed * max_delta_speed) {
            ++invalidCount;
            psis_left_for_plan_B = 10;
            if (local_translation_in_use == IVP_FALSE) {
                startLocalTranslation = true;
                break;
            }
        }

        *input++ = requiredDelta.k[x_idx];
        *input++ = requiredDelta.k[z_idx];
    }

    if (invalidCount == 0 && local_translation_in_use == IVP_TRUE &&
        --psis_left_for_plan_B < 0) {
        for (int index = 0; index < wheel_objects.len(); ++index) {
            delete c_local_ballsocket[index];
            c_local_ballsocket[index] = nullptr;
        }
        local_translation_in_use = IVP_FALSE;
    }

    if (startLocalTranslation) {
        for (int index = 0; index < wheel_objects.len(); ++index) {
            IVP_Constraint_Car_Object *wheel =
                wheel_objects.element_at(index);
            IVP_U_Point bodyAnchor;
            IVP_U_Point worldAnchor;
            IVP_U_Point wheelAnchor;
            bodyAnchor.set(wheel->target_position_bs.get_position());
            worldFromBody->inline_vmult4(&bodyAnchor, &worldAnchor);
            IVP_U_Matrix worldFromWheelObject;
            wheel->real_object->get_m_world_f_object_AT(
                &worldFromWheelObject);
            worldFromWheelObject.inline_vimult4(
                &worldAnchor, &wheelAnchor);
            IVP_U_Point wheelOrigin;
            wheelOrigin.set_to_zero();

            IVP_Template_Constraint definition;
            definition.set_ballsocket_tense_Ros(
                wheel->real_object, &wheelOrigin,
                body_object->real_object, &wheelAnchor);
            c_local_ballsocket[index] =
                environment->create_constraint(&definition);
        }
        local_translation_in_use = IVP_TRUE;
    }

    co_matrix.mult();
    if (local_translation_in_use == IVP_TRUE)
        return;

    IVP_DOUBLE *result = co_matrix.result_vector;
    for (int index = 0; index < wheel_objects.len(); ++index) {
        IVP_Constraint_Car_Object *wheel = wheel_objects.element_at(index);
        IVP_Core *wheelCore = wheel->get_core();
        const IVP_U_Matrix *worldFromWheel =
            wheelCore->get_m_world_f_core_PSI();

        IVP_U_Float_Point impulseBody;
        impulseBody.set_to_zero();
        impulseBody.k[x_idx] = static_cast<IVP_FLOAT>(result[0]);
        impulseBody.k[z_idx] = static_cast<IVP_FLOAT>(result[1]);
        result += 2;
        IVP_U_Float_Point impulseWorld;
        worldFromBody->inline_vmult3(&impulseBody, &impulseWorld);
        IVP_U_Point targetWorld;
        worldFromBody->inline_vmult4(
            &wheel->target_position_bs.vv, &targetWorld);
        bodyCore->push_core_ws(&targetWorld, &impulseWorld);
        impulseWorld.mult(-1.0f);
        wheelCore->push_core_ws(&worldFromWheel->vv, &impulseWorld);
    }
}

inline void IVP_Constraint_Solver_Car_Builder::calc_pushing_behavior(
    int appendedObject, int pushVector) {
    IVP_Constraint_Car_Object *wheel =
        car_solver->wheel_objects.element_at(appendedObject);
    IVP_Core *wheelCore = wheel->get_core();
    IVP_Core *bodyCore = car_solver->body_object->get_core();
    const IVP_U_Matrix *worldFromBody =
        bodyCore->get_m_world_f_core_PSI();

    IVP_U_Matrix worldFromWheel = *worldFromBody;
    worldFromBody->inline_mmult4(
        &wheel->target_position_bs, &worldFromWheel);
    IVP_U_Matrix bodyFromWheel;
    worldFromBody->inline_mimult4(&worldFromWheel, &bodyFromWheel);

    IVP_U_Float_Point impulseBody;
    impulseBody.set_to_zero();
    impulseBody.k[pushVector % 3] = 1.0f;
    IVP_U_Float_Point impulseWorld;
    IVP_U_Float_Point impulseWheel;
    worldFromBody->inline_vmult3(&impulseBody, &impulseWorld);
    worldFromWheel.inline_vimult3(&impulseWorld, &impulseWheel);
    IVP_U_Float_Point inverseImpulseBody;
    IVP_U_Float_Point inverseImpulseWorld;
    IVP_U_Float_Point inverseImpulseWheel;
    inverseImpulseBody.set_negative(&impulseBody);
    worldFromBody->inline_vmult3(
        &inverseImpulseBody, &inverseImpulseWorld);
    worldFromWheel.inline_vimult3(
        &inverseImpulseWorld, &inverseImpulseWheel);

    IVP_U_Float_Point wheelPushPosition;
    wheelPushPosition.set_to_zero();
    IVP_U_Float_Point bodyPushPosition;
    bodyFromWheel.inline_vmult4(
        &wheelPushPosition, &bodyPushPosition);

    IVP_U_Float_Point wheelSpeedBody;
    IVP_U_Float_Point wheelRotationBody;
    IVP_U_Float_Point bodySurfaceBody;
    IVP_U_Float_Point bodyRotationBody;
    IVP_U_Float_Point bodySurfaceWorld;
    IVP_U_Float_Point bodyCenterWorld;

    if (pushVector <= 2) {
        IVP_U_Float_Point wheelCenterWorld;
        IVP_U_Float_Point wheelRotationWheel;
        wheelCore->test_push_core(
            &wheelPushPosition, &impulseWheel, &impulseWorld,
            &wheelCenterWorld, &wheelRotationWheel);
        worldFromBody->inline_vimult3(
            &wheelCenterWorld, &wheelSpeedBody);
        wheelRotationBody.set_to_zero();

        bodyCore->test_push_core(
            &bodyPushPosition, &inverseImpulseBody,
            &inverseImpulseWorld, &bodyCenterWorld,
            &bodyRotationBody);
        bodyCore->get_surface_speed_on_test(
            &bodyPushPosition, &bodyCenterWorld,
            &bodyRotationBody, &bodySurfaceWorld);
        worldFromBody->inline_vimult3(
            &bodySurfaceWorld, &bodySurfaceBody);
    } else {
        IVP_U_Float_Point wheelRotationWheel;
        wheelCore->test_rot_push_core_multiple_cs(
            &impulseWheel, 1.0, &wheelRotationWheel);
        bodyFromWheel.inline_vmult3(
            &wheelRotationWheel, &wheelRotationBody);
        wheelSpeedBody.set_to_zero();

        bodyCore->test_rot_push_core_multiple_cs(
            &impulseBody, -1.0, &bodyRotationBody);
        bodyCenterWorld.set_to_zero();
        bodyCore->get_surface_speed_on_test(
            &bodyPushPosition, &bodyCenterWorld,
            &bodyRotationBody, &bodySurfaceWorld);
        worldFromBody->inline_vimult3(
            &bodySurfaceWorld, &bodySurfaceBody);
    }

    int reducedPush = 0;
    for (int index = 0; index < pushVector; ++index) {
        if (car_solver->constraint_is_disabled[index] == IVP_FALSE)
            ++reducedPush;
    }
    const int matrixColumn =
        n_constraints * appendedObject + reducedPush;
    int matrixRow = 0;
    for (int index = 0; index < car_solver->wheel_objects.len(); ++index) {
        IVP_U_Float_Point deltaSpeed;
        IVP_U_Float_Point deltaRotation;
        if (index == appendedObject) {
            deltaSpeed.subtract(&wheelSpeedBody, &bodySurfaceBody);
            deltaRotation.subtract(
                &wheelRotationBody, &bodyRotationBody);
        } else {
            const IVP_U_Point *otherCenterWorld =
                car_solver->wheel_objects.element_at(index)
                    ->get_core()->get_m_world_f_core_PSI()->get_position();
            IVP_U_Float_Point otherCenterBody;
            IVP_U_Float_Point otherSurfaceWorld;
            worldFromBody->inline_vimult4(
                otherCenterWorld, &otherCenterBody);
            bodyCore->get_surface_speed_on_test(
                &otherCenterBody, &bodyCenterWorld,
                &bodyRotationBody, &otherSurfaceWorld);
            worldFromBody->inline_vimult3(
                &otherSurfaceWorld, &deltaSpeed);
            deltaSpeed.mult(-1.0f);
            deltaRotation.set_negative(&bodyRotationBody);
        }

        for (int axis = 0; axis < 3; ++axis) {
            if (car_solver->constraint_is_disabled[axis] == IVP_FALSE) {
                tmp_matrix.set_value(
                    deltaSpeed.k[axis], matrixColumn, matrixRow++);
            }
        }
        for (int axis = 0; axis < 3; ++axis) {
            if (car_solver->constraint_is_disabled[axis + 3] == IVP_FALSE) {
                tmp_matrix.set_value(
                    deltaRotation.k[axis], matrixColumn, matrixRow++);
            }
        }
    }
}

inline IVP_RETURN_TYPE
IVP_Constraint_Solver_Car_Builder::calc_constraint_matrix() {
    if (n_appends <= 0 || n_constraints <= 0)
        return IVP_FAULT;
    const int size = n_constraints * n_appends;
    const unsigned int matrixBytes = static_cast<unsigned int>(
        size * size * sizeof(IVP_DOUBLE));
    const unsigned int vectorBytes = static_cast<unsigned int>(
        size * sizeof(IVP_DOUBLE));
    auto allocate = [](unsigned int bytes) {
        return static_cast<IVP_DOUBLE *>(BML::IVP::ABI::Invoke<void *>(
            BML::IVP::ABI::Address::Allocate, bytes));
    };

    tmp_matrix.columns = size;
    tmp_matrix.calc_aligned_row_len();
    tmp_matrix.MATRIX_EPS = 1.0e-9;
    tmp_matrix.matrix_values = allocate(matrixBytes);
    tmp_matrix.desired_vector = allocate(vectorBytes);
    tmp_matrix.result_vector = allocate(vectorBytes);
    if (!tmp_matrix.matrix_values || !tmp_matrix.desired_vector ||
        !tmp_matrix.result_vector) {
        IVP_Constraint_Solver_Car::release_matrix_storage(tmp_matrix);
        return IVP_FAULT;
    }
    tmp_matrix.debug_fill_zero();

    for (int appended = 0; appended < n_appends; ++appended) {
        for (int direction = 0; direction < 6; ++direction) {
            if (car_solver->constraint_is_disabled[direction] == IVP_FALSE)
                calc_pushing_behavior(appended, direction);
        }
    }

    car_solver->co_matrix.columns = size;
    car_solver->co_matrix.calc_aligned_row_len();
    car_solver->co_matrix.MATRIX_EPS = 1.0e-9;
    car_solver->co_matrix.matrix_values = allocate(matrixBytes);
    if (!car_solver->co_matrix.matrix_values) {
        IVP_Constraint_Solver_Car::release_matrix_storage(tmp_matrix);
        return IVP_FAULT;
    }
    const IVP_RETURN_TYPE result =
        tmp_matrix.invert(&car_solver->co_matrix);
    IVP_Constraint_Solver_Car::release_matrix_storage(tmp_matrix);

    car_solver->co_matrix.desired_vector = allocate(vectorBytes);
    car_solver->co_matrix.result_vector = allocate(vectorBytes);
    if (!car_solver->co_matrix.desired_vector ||
        !car_solver->co_matrix.result_vector) {
        IVP_Constraint_Solver_Car::release_matrix_storage(
            car_solver->co_matrix);
        return IVP_FAULT;
    }
    return result;
}

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Constraint_Car_Object) == 0xB0);
static_assert(offsetof(IVP_Constraint_Car_Object, real_object) == 0x00);
static_assert(offsetof(IVP_Constraint_Car_Object, target_position_bs) == 0x08);
static_assert(offsetof(IVP_Constraint_Car_Object, last_skid_time) == 0xA0);
static_assert(offsetof(IVP_Constraint_Car_Object, fix_wheel_constraint) == 0xA8);
static_assert(sizeof(IVP_Constraint_Solver_Car) == 0xA0);
static_assert(offsetof(IVP_Constraint_Solver_Car, body_object) == 0x04);
static_assert(offsetof(IVP_Constraint_Solver_Car, co_matrix) == 0x10);
static_assert(offsetof(IVP_Constraint_Solver_Car, x_idx) == 0x30);
static_assert(sizeof(IVP_Constraint_Solver_Car_Builder) == 0x30);
static_assert(
    offsetof(IVP_Constraint_Solver_Car_Builder, car_solver) == 0x00);
static_assert(
    offsetof(IVP_Constraint_Solver_Car_Builder, n_appends) == 0x04);
static_assert(
    offsetof(IVP_Constraint_Solver_Car_Builder, n_constraints) == 0x08);
#endif

#endif // BML_IVP_CONSTRAINT_CAR_H
