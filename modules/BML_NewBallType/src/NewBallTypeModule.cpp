#define BML_LOADER_IMPLEMENTATION
#include "bml_module.hpp"
#include "bml_services.hpp"
#include "bml_newballtype.h"
#include "bml_topics.h"
#include "bml_virtools.h"
#include "bml_virtools.hpp"
#include "bml_virtools_payloads.h"

#include "NewBallTypeMod.h"

namespace {
class NewBallTypeModule : public bml::Module {
    bml::imc::SubscriptionManager m_Subs;
    bml::PublishedInterface m_Published;
    NewBallTypeMod m_Runtime;

    static NewBallTypeModule *GetInstance() {
        return bml::detail::ModuleEntryHelper<NewBallTypeModule>::GetInstance();
    }

    static BML_Result ExtRegisterBallType(const BML_NewBallTypeBallDesc *desc) {
        if (!desc || desc->struct_size < sizeof(BML_NewBallTypeBallDesc) || !desc->ball_file_utf8 ||
            !desc->ball_id_utf8 || !desc->ball_name_utf8 || !desc->object_name_utf8) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *instance = GetInstance();
        if (!instance) {
            return BML_RESULT_NOT_FOUND;
        }

        instance->m_Runtime.RegisterBallType(desc->ball_file_utf8,
                                             desc->ball_id_utf8,
                                             desc->ball_name_utf8,
                                             desc->object_name_utf8,
                                             desc->friction,
                                             desc->elasticity,
                                             desc->mass,
                                             desc->collision_group_utf8 ? desc->collision_group_utf8 : "",
                                             desc->linear_damp,
                                             desc->rotational_damp,
                                             desc->force,
                                             desc->radius);
        return BML_RESULT_OK;
    }

    static BML_Result ExtRegisterFloorType(const BML_NewBallTypeFloorDesc *desc) {
        if (!desc || desc->struct_size < sizeof(BML_NewBallTypeFloorDesc) || !desc->name_utf8) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *instance = GetInstance();
        if (!instance) {
            return BML_RESULT_NOT_FOUND;
        }

        instance->m_Runtime.RegisterFloorType(desc->name_utf8,
                                              desc->friction,
                                              desc->elasticity,
                                              desc->mass,
                                              desc->collision_group_utf8 ? desc->collision_group_utf8 : "",
                                              desc->enable_collision != 0);
        return BML_RESULT_OK;
    }

    static BML_Result ExtRegisterModuleBall(const BML_NewBallTypeModuleBallDesc *desc) {
        if (!desc || desc->struct_size < sizeof(BML_NewBallTypeModuleBallDesc) || !desc->name_utf8) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *instance = GetInstance();
        if (!instance) {
            return BML_RESULT_NOT_FOUND;
        }

        instance->m_Runtime.RegisterModulBall(desc->name_utf8,
                                              desc->fixed != 0,
                                              desc->friction,
                                              desc->elasticity,
                                              desc->mass,
                                              desc->collision_group_utf8 ? desc->collision_group_utf8 : "",
                                              desc->frozen != 0,
                                              desc->enable_collision != 0,
                                              desc->calc_mass_center != 0,
                                              desc->linear_damp,
                                              desc->rotational_damp,
                                              desc->radius);
        return BML_RESULT_OK;
    }

    static BML_Result ExtRegisterModuleConvex(const BML_NewBallTypeModuleConvexDesc *desc) {
        if (!desc || desc->struct_size < sizeof(BML_NewBallTypeModuleConvexDesc) || !desc->name_utf8) {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *instance = GetInstance();
        if (!instance) {
            return BML_RESULT_NOT_FOUND;
        }

        instance->m_Runtime.RegisterModulConvex(desc->name_utf8,
                                                desc->fixed != 0,
                                                desc->friction,
                                                desc->elasticity,
                                                desc->mass,
                                                desc->collision_group_utf8 ? desc->collision_group_utf8 : "",
                                                desc->frozen != 0,
                                                desc->enable_collision != 0,
                                                desc->calc_mass_center != 0,
                                                desc->linear_damp,
                                                desc->rotational_damp);
        return BML_RESULT_OK;
    }

    static BML_Result ExtRegisterTrafo(const char *nameUtf8) {
        if (!nameUtf8 || nameUtf8[0] == '\0') {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *instance = GetInstance();
        if (!instance) {
            return BML_RESULT_NOT_FOUND;
        }

        instance->m_Runtime.RegisterTrafo(nameUtf8);
        return BML_RESULT_OK;
    }

    static BML_Result ExtRegisterModule(const char *nameUtf8) {
        if (!nameUtf8 || nameUtf8[0] == '\0') {
            return BML_RESULT_INVALID_ARGUMENT;
        }

        auto *instance = GetInstance();
        if (!instance) {
            return BML_RESULT_NOT_FOUND;
        }

        instance->m_Runtime.RegisterModul(nameUtf8);
        return BML_RESULT_OK;
    }

public:
    BML_Result OnAttach() override {
        m_Subs = Services().CreateSubscriptions();

        auto *ctx = bml::virtools::GetCKContext(Services());
        m_Runtime.SetServices(&Services());
        m_Runtime.SetContext(ctx);
        m_Runtime.InitObjectLoader(ctx);

        static const BML_NewBallTypeInterface kService = {
            BML_IFACE_HEADER(BML_NewBallTypeInterface, BML_NEWBALLTYPE_INTERFACE_ID, 1, 0),
            &ExtRegisterBallType,
            &ExtRegisterFloorType,
            &ExtRegisterModuleBall,
            &ExtRegisterModuleConvex,
            &ExtRegisterTrafo,
            &ExtRegisterModule
        };

        m_Published = Publish(BML_NEWBALLTYPE_INTERFACE_ID,
                              &kService,
                              1,
                              0,
                              0,
                              BML_INTERFACE_FLAG_IMMUTABLE);
        if (!m_Published) {
            Services().Log().Error("Failed to register NewBallType service");
            return BML_RESULT_FAIL;
        }

        m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_OBJECT, [this](const bml::imc::Message &msg) {
            auto *payload = msg.As<BML_ObjectLoadEvent>();
            if (!payload) {
                return;
            }

            m_Runtime.SetContext(payload->master_object ? payload->master_object->GetCKContext() :
                                                 bml::virtools::GetCKContext(Services()));
            m_Runtime.HandleLoadObject(payload->filename,
                                       payload->is_map,
                                       payload->class_id,
                                       payload->object_ids,
                                       payload->object_count);
        });

        m_Subs.Add(BML_TOPIC_OBJECTLOAD_LOAD_SCRIPT, [this](const bml::imc::Message &msg) {
            auto *payload = msg.As<BML_ScriptLoadEvent>();
            if (!payload || !payload->script) {
                return;
            }

            m_Runtime.SetContext(payload->script->GetCKContext());
            m_Runtime.InitObjectLoader(payload->script->GetCKContext());
            m_Runtime.HandleLoadScript(payload->script);
        });

        return BML_RESULT_OK;
    }

    void OnDetach() override {
        (void) m_Published.Reset();
    }
};
}

BML_DEFINE_MODULE(NewBallTypeModule)
