#include "BuiltinInteropApis.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BML/InteropApi.h"
#include "BML/InteropMath.h"

#include "BMLMod.h"
#include "InteropRegistry.h"
#include "InteropEventSnapshot.h"
#include "InteropObjectReferenceRegistry.h"
#include "Logger.h"
#include "ModContext.h"
#include "BML/Generated/bml_gameplay_api.h"
#include "BML/Generated/bml_events_api.h"
#include "BML/Generated/bml_runtime_api.h"
#include "BML/Generated/bml_scene_api.h"
#include "BML/Generated/bml_ui_api.h"
#include "BML/Generated/bml_events_imc.hpp"
#include "BML/Generated/bml_gameplay_imc.hpp"
#include "BML/Generated/bml_scene_imc.hpp"
#include "BML/Generated/bml_ui_imc.hpp"
#include "BML/Generated/bml_runtime_imc.hpp"

namespace {

namespace RuntimeApi = BML::Interop::Generated::Bml::Runtime;
namespace SceneApi = BML::Interop::Generated::Bml::Scene;
namespace GameplayApi = BML::Interop::Generated::Bml::Gameplay;
namespace UiApi = BML::Interop::Generated::Bml::Ui;
namespace EventsApi = BML::Interop::Generated::Bml::Events;
namespace ImcRuntimeApi = BML::Imc::Generated::Bml::Runtime;
namespace ImcEventsApi = BML::Imc::Generated::Bml::Events;
namespace ImcGameplayApi = BML::Imc::Generated::Bml::Gameplay;
namespace ImcSceneApi = BML::Imc::Generated::Bml::Scene;
namespace ImcUiApi = BML::Imc::Generated::Bml::Ui;

constexpr uint32_t kVirtoolsObjectDomain = BML_INTEROP_OBJECT_DOMAIN_VIRTOOLS;

class ObjectReferences {
public:
    BML_ObjectRef Make(CKObject *object) {
        if (!object || object->GetID() == 0)
            return {};
        return m_References.Make(kVirtoolsObjectDomain,
                                 static_cast<uint32_t>(object->GetID()),
                                 object);
    }

    CKObject *Resolve(ModContext *context, BML_ObjectRef reference) const {
        if (!context || !context->GetCKContext() || reference.Domain != kVirtoolsObjectDomain ||
            reference.Slot == 0 || reference.Generation == 0) {
            return nullptr;
        }
        CKObject *current = context->GetCKContext()->GetObject(static_cast<CK_ID>(reference.Slot));
        return current && !current->IsToBeDeleted() &&
                       m_References.Matches(reference.Slot, reference.Generation, current)
                   ? current
                   : nullptr;
    }

    void Invalidate(const CK_ID *ids, int count) {
        if (!ids || count <= 0)
            return;
        for (int index = 0; index < count; ++index)
            m_References.Invalidate(static_cast<uint32_t>(ids[index]));
    }

    void InvalidateAll() { m_References.InvalidateAll(); }

private:
    InteropObjectReferenceRegistry m_References;
};

class BuiltinInteropProvider {
public:
    explicit BuiltinInteropProvider(BMLMod &mod) : m_Mod(mod) {}

    int Register() {
        ModContext *context = GetContext();
        if (!context)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        BML_InteropProviderCallbacks runtime{};
        runtime.Size = sizeof(runtime);
        runtime.ReadResource = &ReadRuntimeResource;

        BML_InteropProviderCallbacks scene{};
        scene.Size = sizeof(scene);
        scene.ReadComponent = &ReadSceneComponentCallback;
        scene.InvokeQuery = &InvokeSceneQuery;

        BML_InteropProviderCallbacks gameplay{};
        gameplay.Size = sizeof(gameplay);
        gameplay.Probe = &ProbeGameplay;
        gameplay.ReadResource = &ReadGameplayResourceCallback;
        gameplay.ReadCollection = &ReadGameplayCollection;

        BML_InteropProviderCallbacks ui{};
        ui.Size = sizeof(ui);
        ui.ReadResource = &ReadUiResourceCallback;
        ui.InvokeCommand = &InvokeUiCommand;

        BML_InteropProviderCallbacks events{};
        events.Size = sizeof(events);

        BML::InteropRegistry &registry = context->GetInteropRegistry();
        const char *owner = m_Mod.GetID();
        const std::array registrations = {
            std::pair{&RuntimeApi::Descriptor, &runtime},
            std::pair{&SceneApi::Descriptor, &scene},
            std::pair{&GameplayApi::Descriptor, &gameplay},
            std::pair{&UiApi::Descriptor, &ui},
            std::pair{&EventsApi::Descriptor, &events},
        };
        for (size_t index = 0; index < registrations.size(); ++index) {
            const auto &[descriptor, callbacks] = registrations[index];
            const int status = registry.RegisterProvider(owner, descriptor, callbacks, this);
            if (status == BML_OK)
                continue;
            while (index > 0) {
                --index;
                (void)registry.UnregisterProvider(owner, registrations[index].first->ApiId);
            }
            return status;
        }
        const int imcStatus = RegisterImc();
        if (imcStatus != BML_OK) {
            (void)registry.UnregisterProvider(owner, EventsApi::ApiId);
            (void)registry.UnregisterProvider(owner, UiApi::ApiId);
            (void)registry.UnregisterProvider(owner, GameplayApi::ApiId);
            (void)registry.UnregisterProvider(owner, SceneApi::ApiId);
            (void)registry.UnregisterProvider(owner, RuntimeApi::ApiId);
            return imcStatus;
        }
        return BML_OK;
    }

    void Unregister() {
        (void)m_ImcEvents.Close();
        (void)m_ImcUi.Close();
        (void)m_ImcGameplay.Close();
        (void)m_ImcScene.Close();
        (void)m_ImcRuntime.Close();
        ModContext *context = GetContext();
        if (!context)
            return;
        BML::InteropRegistry &registry = context->GetInteropRegistry();
        const char *owner = m_Mod.GetID();
        (void)registry.UnregisterProvider(owner, EventsApi::ApiId);
        (void)registry.UnregisterProvider(owner, UiApi::ApiId);
        (void)registry.UnregisterProvider(owner, GameplayApi::ApiId);
        (void)registry.UnregisterProvider(owner, SceneApi::ApiId);
        (void)registry.UnregisterProvider(owner, RuntimeApi::ApiId);
    }

    ModContext *Context() const { return GetContext(); }

    BML_ObjectRef MakeObjectRef(CKObject *object) {
        return m_ObjectReferences.Make(object);
    }

    CKObject *ResolveObjectRef(BML_ObjectRef reference) const {
        return m_ObjectReferences.Resolve(GetContext(), reference);
    }

    void InvalidateObjectRefs(const CK_ID *ids, int count) { m_ObjectReferences.Invalidate(ids, count); }
    void InvalidateAllObjectRefs() { m_ObjectReferences.InvalidateAll(); }

    bool HasImcEventConsumers() const noexcept {
        std::size_t count = 0;
        return m_ImcEvents.GetAllSubscriberCount(count) == BML_OK && count != 0;
    }

    void PublishEvent(const BML::InteropEventSnapshot &event) {
        PublishImcEvent(event);
        ModContext *context = GetContext();
        if (!context)
            return;
        BML::InteropRegistry &registry = context->GetInteropRegistry();
        if (!registry.HasStreamConsumers(EventsApi::Descriptor.ApiId, "all"))
            return;
        BML_InteropRecordBuilder *record = nullptr;
        if (registry.CreateStreamRecord(m_Mod.GetID(), EventsApi::ApiId, "all", &record) != BML_OK || !record)
            return;
        struct RecordGuard {
            BML::InteropRegistry &Registry;
            BML_InteropRecordBuilder *Record;
            ~RecordGuard() { (void)Registry.DestroyRecordBuilder(Record); }
        } guard{registry, record};

        int status = Set(record, EventsApi::EventField::Kind, BML_INTEROP_FIELD_INT, event.Kind);
        if (status != BML_OK)
            return;
        const bool isLoad = event.Kind == BML_EVENT_LOAD_OBJECT || event.Kind == BML_EVENT_LOAD_SCRIPT;
        const bool isPhysics = event.Kind == BML_EVENT_PHYSICALIZE || event.Kind == BML_EVENT_UNPHYSICALIZE;
        const bool isCommand = event.Kind == BML_EVENT_COMMAND_PRE || event.Kind == BML_EVENT_COMMAND_POST;
        if (isLoad) {
            status = SetString(record, EventsApi::EventField::Filename, event.Filename);
            if (status == BML_OK) status = SetBool(record, EventsApi::EventField::IsMap, event.IsMap);
            if (status == BML_OK) status = SetString(record, EventsApi::EventField::MasterName, event.MasterName);
            if (status == BML_OK) status = Set(record, EventsApi::EventField::FilterClass, BML_INTEROP_FIELD_INT, event.FilterClass);
            if (status == BML_OK) status = SetBool(record, EventsApi::EventField::AddToScene, event.AddToScene);
            if (status == BML_OK) status = SetBool(record, EventsApi::EventField::ReuseMeshes, event.ReuseMeshes);
            if (status == BML_OK) status = SetBool(record, EventsApi::EventField::ReuseMaterials, event.ReuseMaterials);
            if (status == BML_OK) status = SetBool(record, EventsApi::EventField::Dynamic, event.IsDynamic);
            if (status == BML_OK && event.Kind == BML_EVENT_LOAD_OBJECT) {
                status = SetArray(record, EventsApi::EventField::ObjectIds,
                                  BML_INTEROP_FIELD_OBJECT_ARRAY, event.ObjectIds);
                if (status == BML_OK) status = Set(record, EventsApi::EventField::MasterObject, BML_INTEROP_FIELD_OBJECT,
                                                    event.MasterObject);
            }
            if (status == BML_OK && event.Kind == BML_EVENT_LOAD_SCRIPT)
                status = Set(record, EventsApi::EventField::Script, BML_INTEROP_FIELD_OBJECT, event.Script);
        }
        if (status == BML_OK && isPhysics) {
            status = Set(record, EventsApi::EventField::Target, BML_INTEROP_FIELD_OBJECT, event.Target);
            if (status == BML_OK && event.Kind == BML_EVENT_PHYSICALIZE) {
                status = SetBool(record, EventsApi::EventField::Fixed, event.Fixed);
                if (status == BML_OK) status = Set(record, EventsApi::EventField::Friction, BML_INTEROP_FIELD_FLOAT, event.Friction);
                if (status == BML_OK) status = Set(record, EventsApi::EventField::Elasticity, BML_INTEROP_FIELD_FLOAT, event.Elasticity);
                if (status == BML_OK) status = Set(record, EventsApi::EventField::Mass, BML_INTEROP_FIELD_FLOAT, event.Mass);
                if (status == BML_OK) status = SetString(record, EventsApi::EventField::CollisionGroup, event.CollisionGroup);
                if (status == BML_OK) status = SetBool(record, EventsApi::EventField::StartFrozen, event.StartFrozen);
                if (status == BML_OK) status = SetBool(record, EventsApi::EventField::EnableCollision, event.EnableCollision);
                if (status == BML_OK) status = SetBool(record, EventsApi::EventField::AutoCalculateMassCenter, event.AutoCalculateMassCenter);
                if (status == BML_OK) status = Set(record, EventsApi::EventField::LinearDamp, BML_INTEROP_FIELD_FLOAT, event.LinearDamp);
                if (status == BML_OK) status = Set(record, EventsApi::EventField::RotDamp, BML_INTEROP_FIELD_FLOAT, event.RotDamp);
                if (status == BML_OK) status = SetString(record, EventsApi::EventField::CollisionSurface, event.CollisionSurface);
                if (status == BML_OK) status = Set(record, EventsApi::EventField::MassCenter, BML_INTEROP_FIELD_VEC3, event.MassCenter);
                if (status == BML_OK) status = SetArray(record, EventsApi::EventField::ConvexMeshes,
                                                        BML_INTEROP_FIELD_OBJECT_ARRAY, event.ConvexMeshes);
                if (status == BML_OK) status = SetArray(record, EventsApi::EventField::BallCenters,
                                                        BML_INTEROP_FIELD_VEC3_ARRAY, event.BallCenters);
                if (status == BML_OK) status = SetArray(record, EventsApi::EventField::BallRadii,
                                                        BML_INTEROP_FIELD_FLOAT_ARRAY, event.BallRadii);
                if (status == BML_OK) status = SetArray(record, EventsApi::EventField::ConcaveMeshes,
                                                        BML_INTEROP_FIELD_OBJECT_ARRAY, event.ConcaveMeshes);
            }
        }
        if (status == BML_OK && isCommand) {
            status = SetString(record, EventsApi::EventField::Command, event.Command);
            if (status == BML_OK) status = SetStringArray(record, EventsApi::EventField::CommandArgs, event.CommandArgs);
        }
        if (status == BML_OK && event.Kind == BML_EVENT_CONFIG_MODIFIED) {
            status = SetString(record, EventsApi::EventField::ConfigCategory, event.ConfigCategory);
            if (status == BML_OK) status = SetString(record, EventsApi::EventField::ConfigKey, event.ConfigKey);
            if (status == BML_OK) status = Set(record, EventsApi::EventField::ConfigType, BML_INTEROP_FIELD_INT, event.ConfigType);
            if (status == BML_OK) status = SetString(record, EventsApi::EventField::ConfigValue, event.ConfigValue);
        }
        if (status == BML_OK && event.Kind == BML_EVENT_CHEAT_CHANGED)
            status = SetBool(record, EventsApi::EventField::CheatEnabled, event.CheatEnabled);
        if (status == BML_OK)
            (void)registry.Publish(m_Mod.GetID(), record);
    }

private:
    int RegisterImc() {
        const char *owner = m_Mod.GetID();
        int status = m_ImcRuntime.Open(owner);
        if (status == BML_OK) status = m_ImcRuntime.RegisterState(&ReadImcRuntimeState, this);
        if (status == BML_OK) status = m_ImcRuntime.RegisterClock(&ReadImcRuntimeClock, this);
        if (status == BML_OK) status = m_ImcRuntime.RegisterScore(&ReadImcRuntimeScore, this);
        if (status == BML_OK) status = m_ImcScene.Open(owner);
        if (status == BML_OK) status = m_ImcScene.RegisterObject(&ReadImcSceneObject, this);
        if (status == BML_OK) status = m_ImcScene.RegisterEntity(&ReadImcSceneEntity, this);
        if (status == BML_OK) status = m_ImcScene.RegisterFindName(&ReadImcSceneFindName, this);
        if (status == BML_OK) status = m_ImcScene.RegisterFindNameClass(&ReadImcSceneFindNameClass, this);
        if (status == BML_OK) status = m_ImcGameplay.Open(owner);
        if (status == BML_OK) status = m_ImcGameplay.RegisterLevel(&ReadImcGameplayLevel, this);
        if (status == BML_OK) status = m_ImcGameplay.RegisterEnergy(&ReadImcGameplayEnergy, this);
        if (status == BML_OK) status = m_ImcGameplay.RegisterCatalog(&ReadImcGameplayCatalog, this);
        if (status == BML_OK) status = m_ImcGameplay.RegisterCheckpoints(&ReadImcGameplayCheckpoints, this);
        if (status == BML_OK) status = m_ImcGameplay.RegisterResetpoints(&ReadImcGameplayResetpoints, this);
        if (status == BML_OK) status = m_ImcUi.Open(owner);
        if (status == BML_OK) status = RegisterImcUi();
        if (status == BML_OK) status = m_ImcEvents.Open(owner);
        if (status != BML_OK) {
            (void)m_ImcEvents.Close();
            (void)m_ImcUi.Close();
            (void)m_ImcGameplay.Close();
            (void)m_ImcScene.Close();
            (void)m_ImcRuntime.Close();
        }
        return status;
    }

    static int ReadImcRuntimeState(ImcRuntimeApi::RuntimeStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context) return BML_ERROR_INTEROP_UNSUPPORTED;
        out.InGame = context->IsIngame(); out.InLevel = context->IsInLevel();
        out.Paused = context->IsPaused(); out.Playing = context->IsPlaying();
        out.CheatEnabled = context->IsCheatEnabled(); return BML_OK;
    }

    static int ReadImcRuntimeClock(ImcRuntimeApi::ClockStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        CKTimeManager *time = context ? context->GetTimeManager() : nullptr;
        if (!time) return BML_ERROR_INTEROP_UNSUPPORTED;
        out.TimeMs = time->GetTime(); out.AbsoluteMs = time->GetAbsoluteTime(); out.DeltaMs = time->GetLastDeltaTime();
        const CKDWORD tick = time->GetMainTickCount();
        out.Frame = tick > static_cast<CKDWORD>((std::numeric_limits<int>::max)())
                        ? (std::numeric_limits<int>::max)() : static_cast<int>(tick);
        return BML_OK;
    }

    static int ReadImcRuntimeScore(ImcRuntimeApi::ScoreStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        if (!provider) return BML_ERROR_INVALID_PARAMETER;
        out.Sr = provider->m_Mod.GetSRScore(); out.Hs = provider->m_Mod.GetHSScore(); return BML_OK;
    }

    void PublishImcEvent(const BML::InteropEventSnapshot &event) {
        ImcEventsApi::EventValue value{}; value.Kind = event.Kind;
        const bool isLoad = event.Kind == BML_EVENT_LOAD_OBJECT || event.Kind == BML_EVENT_LOAD_SCRIPT;
        const bool isPhysics = event.Kind == BML_EVENT_PHYSICALIZE || event.Kind == BML_EVENT_UNPHYSICALIZE;
        const bool isCommand = event.Kind == BML_EVENT_COMMAND_PRE || event.Kind == BML_EVENT_COMMAND_POST;
        if (isLoad) {
            value.HasFilename = true; value.Filename = event.Filename;
            value.HasIsMap = true; value.IsMap = event.IsMap;
            value.HasMasterName = true; value.MasterName = event.MasterName;
            value.HasFilterClass = true; value.FilterClass = event.FilterClass;
            value.HasAddToScene = true; value.AddToScene = event.AddToScene;
            value.HasReuseMeshes = true; value.ReuseMeshes = event.ReuseMeshes;
            value.HasReuseMaterials = true; value.ReuseMaterials = event.ReuseMaterials;
            value.HasDynamic = true; value.Dynamic = event.IsDynamic;
            if (event.Kind == BML_EVENT_LOAD_OBJECT) {
                value.HasObjectIds = true; value.ObjectIds = event.ObjectIds;
                value.HasMasterObject = true; value.MasterObject = event.MasterObject;
            } else {
                value.HasScript = true; value.Script = event.Script;
            }
        }
        if (isPhysics) {
            value.HasTarget = true; value.Target = event.Target;
            if (event.Kind == BML_EVENT_PHYSICALIZE) {
                value.HasFixed = true; value.Fixed = event.Fixed;
                value.HasFriction = true; value.Friction = event.Friction;
                value.HasElasticity = true; value.Elasticity = event.Elasticity;
                value.HasMass = true; value.Mass = event.Mass;
                value.HasCollisionGroup = true; value.CollisionGroup = event.CollisionGroup;
                value.HasStartFrozen = true; value.StartFrozen = event.StartFrozen;
                value.HasEnableCollision = true; value.EnableCollision = event.EnableCollision;
                value.HasAutoCalculateMassCenter = true; value.AutoCalculateMassCenter = event.AutoCalculateMassCenter;
                value.HasLinearDamp = true; value.LinearDamp = event.LinearDamp;
                value.HasRotDamp = true; value.RotDamp = event.RotDamp;
                value.HasCollisionSurface = true; value.CollisionSurface = event.CollisionSurface;
                value.HasMassCenter = true; value.MassCenter = event.MassCenter;
                value.HasConvexMeshes = true; value.ConvexMeshes = event.ConvexMeshes;
                value.HasBallCenters = true; value.BallCenters = event.BallCenters;
                value.HasBallRadii = true; value.BallRadii = event.BallRadii;
                value.HasConcaveMeshes = true; value.ConcaveMeshes = event.ConcaveMeshes;
            }
        }
        if (isCommand) {
            value.HasCommand = true; value.Command = event.Command;
            value.HasCommandArgs = true; value.CommandArgs = event.CommandArgs;
        }
        if (event.Kind == BML_EVENT_CONFIG_MODIFIED) {
            value.HasConfigCategory = true; value.ConfigCategory = event.ConfigCategory;
            value.HasConfigKey = true; value.ConfigKey = event.ConfigKey;
            value.HasConfigType = true; value.ConfigType = event.ConfigType;
            value.HasConfigValue = true; value.ConfigValue = event.ConfigValue;
        }
        if (event.Kind == BML_EVENT_CHEAT_CHANGED) {
            value.HasCheatEnabled = true; value.CheatEnabled = event.CheatEnabled;
        }
        (void)m_ImcEvents.PublishAll(value);
    }

    static int ReadImcSceneObject(const BML_ObjectRef &reference, ImcSceneApi::ObjectInfoValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        CKObject *object = provider ? provider->m_ObjectReferences.Resolve(provider->GetContext(), reference) : nullptr;
        if (!object) return BML_ERROR_INTEROP_OBJECT_INVALID;
        out.Id = static_cast<int>(object->GetID()); out.Name = object->GetName() ? object->GetName() : "";
        out.ClassId = static_cast<int>(object->GetClassID()); out.Visible = object->IsVisible() != FALSE;
        out.Dynamic = object->IsDynamic() != FALSE; return BML_OK;
    }

    static int ReadImcSceneEntity(const BML_ObjectRef &reference, ImcSceneApi::EntityTransformValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        CKObject *object = provider ? provider->m_ObjectReferences.Resolve(provider->GetContext(), reference) : nullptr;
        CK3dEntity *entity = dynamic_cast<CK3dEntity *>(object);
        if (!entity) return BML_ERROR_INTEROP_OBJECT_INVALID;
        VxVector position, scale; entity->GetPosition(&position); entity->GetScale(&scale);
        out.Position = BML::ToVec3(position); out.Scale = BML::ToVec3(scale);
        out.Parent = provider->m_ObjectReferences.Make(entity->GetParent());
        out.ChildCount = entity->GetChildrenCount(); return BML_OK;
    }

    static int ReadImcSceneFindName(const ImcSceneApi::FindNameRequestValue &input,
                                    ImcSceneApi::FindResultValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !context->GetCKContext()) return BML_ERROR_INTEROP_UNSUPPORTED;
        out.Object = provider->m_ObjectReferences.Make(context->GetCKContext()->GetObjectByName(const_cast<char *>(input.Name.c_str())));
        return BML_OK;
    }

    static int ReadImcSceneFindNameClass(const ImcSceneApi::FindNameClassRequestValue &input,
                                         ImcSceneApi::FindResultValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !context->GetCKContext()) return BML_ERROR_INTEROP_UNSUPPORTED;
        out.Object = provider->m_ObjectReferences.Make(
            context->GetCKContext()->GetObjectByNameAndClass(const_cast<char *>(input.Name.c_str()), static_cast<CK_CLASSID>(input.ClassId)));
        return BML_OK;
    }

    static int ReadImcGameplayLevel(ImcGameplayApi::LevelStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("level")) return BML_ERROR_INTEROP_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("CurrentLevel");
        int id = 0, points = 0; VxMatrix matrix;
        if (!ReadValue(array, 0, 0, id) || !ReadMatrix(array, 0, 3, matrix) || !ReadValue(array, 0, 5, points))
            return BML_ERROR_INTEROP_UNSUPPORTED;
        out.Id = id; out.ActiveBall = provider->m_ObjectReferences.Make(ReadObject(array, 0, 1));
        out.ResetMatrix = BML::ToMat4(matrix); out.Points = points; return BML_OK;
    }

    static int ReadImcGameplayEnergy(ImcGameplayApi::EnergyStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("energy")) return BML_ERROR_INTEROP_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("Energy");
        if (!ReadValue(array, 0, 0, out.Points) || !ReadValue(array, 0, 1, out.Lives) ||
            !ReadValue(array, 0, 2, out.StartPoints) || !ReadValue(array, 0, 3, out.StartLives) ||
            !ReadValue(array, 0, 4, out.TimeFactor) || !ReadValue(array, 0, 5, out.LifeBonus))
            return BML_ERROR_INTEROP_UNSUPPORTED;
        return BML_OK;
    }

    static int ReadImcGameplayCatalog(std::vector<ImcGameplayApi::CatalogEntryValue> &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("catalog")) return BML_ERROR_INTEROP_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("AllLevel"); out.clear(); out.reserve(array->GetRowCount());
        for (int row = 0; row < array->GetRowCount(); ++row) {
            ImcGameplayApi::CatalogEntryValue value{};
            if (!ReadString(array, row, 0, value.File) || !ReadString(array, row, 1, value.StartBall) ||
                !ReadString(array, row, 2, value.Sky) || !ReadValue(array, row, 3, value.Bonus) ||
                !ReadValue(array, row, 4, value.Music)) return BML_ERROR_INTEROP_UNSUPPORTED;
            out.push_back(std::move(value));
        }
        return BML_OK;
    }

    static int ReadImcGameplayCheckpoints(std::vector<ImcGameplayApi::CheckpointValue> &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("checkpoints")) return BML_ERROR_INTEROP_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("Checkpoints"); out.clear(); out.reserve(array->GetRowCount());
        for (int row = 0; row < array->GetRowCount(); ++row) {
            VxMatrix matrix; if (!ReadMatrix(array, row, 0, matrix)) return BML_ERROR_INTEROP_UNSUPPORTED;
            ImcGameplayApi::CheckpointValue value{}; value.Matrix = BML::ToMat4(matrix);
            value.Object = provider->m_ObjectReferences.Make(ReadObject(array, row, 1)); out.push_back(value);
        }
        return BML_OK;
    }

    static int ReadImcGameplayResetpoints(std::vector<ImcGameplayApi::ResetpointValue> &out, void *userdata) {
        auto *provider = static_cast<BuiltinInteropProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("resetpoints")) return BML_ERROR_INTEROP_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("ResetPoints"); out.clear(); out.reserve(array->GetRowCount());
        for (int row = 0; row < array->GetRowCount(); ++row) {
            ImcGameplayApi::ResetpointValue value{};
            value.Object = provider->m_ObjectReferences.Make(ReadObject(array, row, 0)); out.push_back(value);
        }
        return BML_OK;
    }
    int RegisterImcUi() {
        int status = m_ImcUi.RegisterMessageAdd(&ImcUiMessageAdd, this);
        if (status == BML_OK) status = m_ImcUi.RegisterMessageClear(&ImcUiMessageClear, this);
        if (status == BML_OK) status = m_ImcUi.RegisterModsMenuOpen(&ImcUiModsMenuOpen, this);
        if (status == BML_OK) status = m_ImcUi.RegisterModsMenuClose(&ImcUiModsMenuClose, this);
        if (status == BML_OK) status = m_ImcUi.RegisterMapMenuOpen(&ImcUiMapMenuOpen, this);
        if (status == BML_OK) status = m_ImcUi.RegisterMapMenuClose(&ImcUiMapMenuClose, this);
        if (status == BML_OK) status = m_ImcUi.RegisterHudSet(&ImcUiHudSet, this);
        if (status == BML_OK) status = m_ImcUi.RegisterHudTitleShow(&ImcUiHudTitleShow, this);
        if (status == BML_OK) status = m_ImcUi.RegisterHudFpsShow(&ImcUiHudFpsShow, this);
        if (status == BML_OK) status = m_ImcUi.RegisterHudSrShow(&ImcUiHudSrShow, this);
        if (status == BML_OK) status = m_ImcUi.RegisterHudSrStart(&ImcUiHudSrStart, this);
        if (status == BML_OK) status = m_ImcUi.RegisterHudSrPause(&ImcUiHudSrPause, this);
        if (status == BML_OK) status = m_ImcUi.RegisterHudSrReset(&ImcUiHudSrReset, this);
        if (status == BML_OK) status = m_ImcUi.RegisterState(&ReadImcUiState, this);
        return status;
    }

    static BuiltinInteropProvider *ImcUiProvider(void *userdata) {
        return static_cast<BuiltinInteropProvider *>(userdata);
    }

    static int ImcUiMessageAdd(const ImcUiApi::MessageInputValue &input,
                               ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.AddIngameMessage(input.Message.c_str()); return BML_OK;
    }
    static int ImcUiMessageClear(const ImcUiApi::EmptyInputValue &, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.ClearIngameMessages(); return BML_OK;
    }
    static int ImcUiModsMenuOpen(const ImcUiApi::EmptyInputValue &, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.OpenModsMenu(); return BML_OK;
    }
    static int ImcUiModsMenuClose(const ImcUiApi::EmptyInputValue &, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.CloseModsMenu(); return BML_OK;
    }
    static int ImcUiMapMenuOpen(const ImcUiApi::EmptyInputValue &, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.OpenMapMenu(); return BML_OK;
    }
    static int ImcUiMapMenuClose(const ImcUiApi::EmptyInputValue &, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.CloseMapMenu(); return BML_OK;
    }
    static int ImcUiHudSet(const ImcUiApi::HudModeInputValue &input, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.SetHUD(input.Mode); return BML_OK;
    }
    static int ImcUiHudTitleShow(const ImcUiApi::VisibleInputValue &input, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.ShowTitle(input.Visible); return BML_OK;
    }
    static int ImcUiHudFpsShow(const ImcUiApi::VisibleInputValue &input, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.ShowFPS(input.Visible); return BML_OK;
    }
    static int ImcUiHudSrShow(const ImcUiApi::VisibleInputValue &input, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.ShowSRTimer(input.Visible); return BML_OK;
    }
    static int ImcUiHudSrStart(const ImcUiApi::EmptyInputValue &, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.StartSRTimer(); return BML_OK;
    }
    static int ImcUiHudSrPause(const ImcUiApi::EmptyInputValue &, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.PauseSRTimer(); return BML_OK;
    }
    static int ImcUiHudSrReset(const ImcUiApi::EmptyInputValue &, ImcUiApi::CommandResultValue &, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        provider->m_Mod.ResetSRTimer(); return BML_OK;
    }
    static int ReadImcUiState(ImcUiApi::HudStateValue &out, void *userdata) {
        auto *provider = ImcUiProvider(userdata); if (!provider) return BML_ERROR_INVALID_PARAMETER;
        out.Mode = provider->m_Mod.GetHUD(); out.SrTime = provider->m_Mod.GetSRTime(); return BML_OK;
    }
    ModContext *GetContext() const { return m_Mod.GetRuntimeContext(); }

    template <typename T>
    int Set(BML_InteropRecordBuilder *record, uint32_t field, BML_INTEROP_FIELD_TYPE type, const T &value) {
        ModContext *context = GetContext();
        return context ? context->GetInteropRegistry().BuilderSetValue(record, field, type, &value, 1)
                       : BML_ERROR_INTEROP_UNSUPPORTED;
    }

    int SetBool(BML_InteropRecordBuilder *record, uint32_t field, bool value) {
        const int raw = value ? 1 : 0;
        return Set(record, field, BML_INTEROP_FIELD_BOOL, raw);
    }

    int SetString(BML_InteropRecordBuilder *record, uint32_t field, const std::string &value) {
        ModContext *context = GetContext();
        return context ? context->GetInteropRegistry().BuilderSetValue(record, field, BML_INTEROP_FIELD_STRING,
                                                                         value.data(), value.size())
                       : BML_ERROR_INTEROP_UNSUPPORTED;
    }

    int SetString(BML_InteropRecordBuilder *record, uint32_t field, const char *value) {
        return SetString(record, field, value ? std::string(value) : std::string());
    }

    template <typename T>
    int SetArray(BML_InteropRecordBuilder *record,
                 uint32_t field,
                 BML_INTEROP_FIELD_TYPE type,
                 const std::vector<T> &values) {
        ModContext *context = GetContext();
        return context ? context->GetInteropRegistry().BuilderSetValue(record, field, type,
                                                                         values.empty() ? nullptr : values.data(), values.size())
                       : BML_ERROR_INTEROP_UNSUPPORTED;
    }

    int SetStringArray(BML_InteropRecordBuilder *record, uint32_t field, const std::vector<std::string> &values) {
        ModContext *context = GetContext();
        if (!context)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        try {
            std::vector<const char *> pointers;
            std::vector<size_t> sizes;
            pointers.reserve(values.size());
            sizes.reserve(values.size());
            for (const std::string &value : values) {
                pointers.push_back(value.data());
                sizes.push_back(value.size());
            }
            return context->GetInteropRegistry().BuilderSetStringArray(record, field,
                                                                          pointers.empty() ? nullptr : pointers.data(),
                                                                          sizes.empty() ? nullptr : sizes.data(),
                                                                          values.size());
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
    }

    int ReadRuntime(BML_InteropRecordBuilder *record, const char *endpoint) {
        ModContext *context = GetContext();
        if (!context)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        if (std::strcmp(endpoint, "state") == 0) {
            int status = SetBool(record, RuntimeApi::RuntimeStateField::InGame, context->IsIngame());
            if (status == BML_OK) status = SetBool(record, RuntimeApi::RuntimeStateField::InLevel, context->IsInLevel());
            if (status == BML_OK) status = SetBool(record, RuntimeApi::RuntimeStateField::Paused, context->IsPaused());
            if (status == BML_OK) status = SetBool(record, RuntimeApi::RuntimeStateField::Playing, context->IsPlaying());
            if (status == BML_OK) status = SetBool(record, RuntimeApi::RuntimeStateField::CheatEnabled, context->IsCheatEnabled());
            return status;
        }
        CKTimeManager *time = context->GetTimeManager();
        if (std::strcmp(endpoint, "clock") == 0) {
            if (!time)
                return BML_ERROR_INTEROP_UNSUPPORTED;
            int status = Set(record, RuntimeApi::ClockStateField::TimeMs, BML_INTEROP_FIELD_FLOAT, time->GetTime());
            if (status == BML_OK) status = Set(record, RuntimeApi::ClockStateField::AbsoluteMs, BML_INTEROP_FIELD_FLOAT, time->GetAbsoluteTime());
            if (status == BML_OK) status = Set(record, RuntimeApi::ClockStateField::DeltaMs, BML_INTEROP_FIELD_FLOAT, time->GetLastDeltaTime());
            if (status == BML_OK) {
                const CKDWORD tick = time->GetMainTickCount();
                const int frame = tick > static_cast<CKDWORD>((std::numeric_limits<int>::max)())
                                      ? (std::numeric_limits<int>::max)()
                                      : static_cast<int>(tick);
                status = Set(record, RuntimeApi::ClockStateField::Frame, BML_INTEROP_FIELD_INT, frame);
            }
            return status;
        }
        if (std::strcmp(endpoint, "score") == 0) {
            int status = Set(record, RuntimeApi::ScoreStateField::Sr, BML_INTEROP_FIELD_FLOAT, m_Mod.GetSRScore());
            if (status == BML_OK) status = Set(record, RuntimeApi::ScoreStateField::Hs, BML_INTEROP_FIELD_INT, m_Mod.GetHSScore());
            return status;
        }
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    }

    int ReadSceneComponent(BML_InteropRecordBuilder *record, const BML_InteropProviderRequest *request) {
        CKObject *object = m_ObjectReferences.Resolve(GetContext(), request->Object);
        if (!object)
            return BML_ERROR_INTEROP_OBJECT_INVALID;
        if (std::strcmp(request->Endpoint, "object") == 0) {
            int status = Set(record, SceneApi::ObjectInfoField::Id, BML_INTEROP_FIELD_INT, static_cast<int>(object->GetID()));
            if (status == BML_OK) status = SetString(record, SceneApi::ObjectInfoField::Name, object->GetName());
            if (status == BML_OK) status = Set(record, SceneApi::ObjectInfoField::ClassId, BML_INTEROP_FIELD_INT, static_cast<int>(object->GetClassID()));
            if (status == BML_OK) status = SetBool(record, SceneApi::ObjectInfoField::Visible, object->IsVisible() != FALSE);
            if (status == BML_OK) status = SetBool(record, SceneApi::ObjectInfoField::Dynamic, object->IsDynamic() != FALSE);
            return status;
        }
        if (std::strcmp(request->Endpoint, "entity") == 0) {
            CK3dEntity *entity = dynamic_cast<CK3dEntity *>(object);
            if (!entity)
                return BML_ERROR_INTEROP_OBJECT_INVALID;
            VxVector position;
            VxVector scale;
            entity->GetPosition(&position);
            entity->GetScale(&scale);
            int status = Set(record, SceneApi::EntityTransformField::Position, BML_INTEROP_FIELD_VEC3, BML::ToVec3(position));
            if (status == BML_OK) status = Set(record, SceneApi::EntityTransformField::Scale, BML_INTEROP_FIELD_VEC3, BML::ToVec3(scale));
            if (status == BML_OK) status = Set(record, SceneApi::EntityTransformField::Parent, BML_INTEROP_FIELD_OBJECT,
                                                m_ObjectReferences.Make(entity->GetParent()));
            if (status == BML_OK) status = Set(record, SceneApi::EntityTransformField::ChildCount,
                                                BML_INTEROP_FIELD_INT, entity->GetChildrenCount());
            return status;
        }
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    }

    int ReadSceneQuery(BML_InteropRecordBuilder *record, const BML_InteropProviderRequest *request) {
        ModContext *context = GetContext();
        if (!context || !context->GetCKContext() || !request->Input)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        std::string name;
        int status = ReadViewString(request->Input, 1, name);
        if (status != BML_OK)
            return status;
        CKObject *object = nullptr;
        if (std::strcmp(request->Endpoint, "find_name") == 0) {
            object = context->GetCKContext()->GetObjectByName(name.data());
        } else if (std::strcmp(request->Endpoint, "find_name_class") == 0) {
            int classId = 0;
            status = context->GetInteropRegistry().RecordViewGetInt(request->Input, 2, &classId);
            if (status != BML_OK)
                return status;
            object = context->GetCKContext()->GetObjectByNameAndClass(name.data(), static_cast<CK_CLASSID>(classId));
        } else {
            return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
        }
        return Set(record, SceneApi::FindResultField::Object, BML_INTEROP_FIELD_OBJECT, m_ObjectReferences.Make(object));
    }

    static bool HasColumns(CKDataArray *array, std::initializer_list<const char *> columns) {
        if (!array || array->GetColumnCount() < static_cast<int>(columns.size()))
            return false;
        int index = 0;
        for (const char *expected : columns) {
            const char *actual = array->GetColumnName(index++);
            if (!actual || std::strcmp(actual, expected) != 0)
                return false;
        }
        return true;
    }

    bool ProbeGameplaySource(const char *endpoint) const {
        ModContext *context = GetContext();
        if (!context)
            return false;
        if (std::strcmp(endpoint, "level") == 0)
            return HasColumns(context->GetArrayByName("CurrentLevel"),
                              {"Level ID", "ActiveBall", "Ball_Pos_Frame", "CurrentResetpoint", "Activation Phase?", "Points"});
        if (std::strcmp(endpoint, "energy") == 0)
            return HasColumns(context->GetArrayByName("Energy"),
                              {"Points", "Lifes", "StartPoints", "StartLifes", "Timefactor", "LifeBonus"});
        if (std::strcmp(endpoint, "catalog") == 0)
            return HasColumns(context->GetArrayByName("AllLevel"),
                              {"Levelfile", "StartBall", "Sky", "LevelBonus", "Music"});
        if (std::strcmp(endpoint, "checkpoints") == 0)
            return HasColumns(context->GetArrayByName("Checkpoints"), {"Matrix", "Object"});
        if (std::strcmp(endpoint, "resetpoints") == 0)
            return HasColumns(context->GetArrayByName("ResetPoints"), {"Resetpoint"});
        return false;
    }

    static bool HasCell(CKDataArray *array, int row, int column) {
        return array && row >= 0 && column >= 0 && row < array->GetRowCount() && column < array->GetColumnCount();
    }

    template <typename T>
    static bool ReadValue(CKDataArray *array, int row, int column, T &outValue) {
        return HasCell(array, row, column) && array->GetElementValue(row, column, &outValue) != 0;
    }

    static bool ReadMatrix(CKDataArray *array, int row, int column, VxMatrix &outValue) {
        if (!HasCell(array, row, column))
            return false;
        CKObject *cell = array->GetElementObject(row, column);
        CKParameter *parameter = dynamic_cast<CKParameter *>(cell);
        return parameter && parameter->GetValue(&outValue) == CK_OK;
    }

    static CKObject *ReadObject(CKDataArray *array, int row, int column) {
        if (!HasCell(array, row, column))
            return nullptr;
        CKObject *cell = array->GetElementObject(row, column);
        if (CKParameter *parameter = dynamic_cast<CKParameter *>(cell))
            return parameter->GetValueObject();
        return cell;
    }

    static bool ReadString(CKDataArray *array, int row, int column, std::string &outValue) {
        if (!HasCell(array, row, column))
            return false;
        const int required = array->GetElementStringValue(row, column, nullptr);
        if (required <= 0)
            return false;
        std::vector<char> buffer(static_cast<size_t>(required), '\0');
        if (array->GetElementStringValue(row, column, buffer.data()) <= 0)
            return false;
        outValue = buffer.data();
        return true;
    }

    int ReadGameplayResource(BML_InteropRecordBuilder *record, const char *endpoint) {
        ModContext *context = GetContext();
        if (!context)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        if (std::strcmp(endpoint, "level") == 0) {
            CKDataArray *array = context->GetArrayByName("CurrentLevel");
            int id = 0;
            VxMatrix matrix;
            if (!ReadValue(array, 0, 0, id) || !ReadMatrix(array, 0, 3, matrix))
                return BML_ERROR_INTEROP_UNSUPPORTED;
            int status = Set(record, GameplayApi::LevelStateField::Id, BML_INTEROP_FIELD_INT, id);
            if (status == BML_OK) status = Set(record, GameplayApi::LevelStateField::ActiveBall, BML_INTEROP_FIELD_OBJECT,
                                                m_ObjectReferences.Make(ReadObject(array, 0, 1)));
            if (status == BML_OK) status = Set(record, GameplayApi::LevelStateField::ResetMatrix, BML_INTEROP_FIELD_MAT4,
                                                BML::ToMat4(matrix));
            if (status == BML_OK) {
                int points = 0;
                if (!ReadValue(array, 0, 5, points)) return BML_ERROR_INTEROP_UNSUPPORTED;
                status = Set(record, GameplayApi::LevelStateField::Points, BML_INTEROP_FIELD_INT, points);
            }
            return status;
        }
        if (std::strcmp(endpoint, "energy") == 0) {
            CKDataArray *array = context->GetArrayByName("Energy");
            int points = 0, lives = 0, startPoints = 0, startLives = 0, lifeBonus = 0;
            float timeFactor = 0.0f;
            if (!ReadValue(array, 0, 0, points) || !ReadValue(array, 0, 1, lives) ||
                !ReadValue(array, 0, 2, startPoints) || !ReadValue(array, 0, 3, startLives) ||
                !ReadValue(array, 0, 4, timeFactor) || !ReadValue(array, 0, 5, lifeBonus)) {
                return BML_ERROR_INTEROP_UNSUPPORTED;
            }
            int status = Set(record, GameplayApi::EnergyStateField::Points, BML_INTEROP_FIELD_INT, points);
            if (status == BML_OK) status = Set(record, GameplayApi::EnergyStateField::Lives, BML_INTEROP_FIELD_INT, lives);
            if (status == BML_OK) status = Set(record, GameplayApi::EnergyStateField::StartPoints, BML_INTEROP_FIELD_INT, startPoints);
            if (status == BML_OK) status = Set(record, GameplayApi::EnergyStateField::StartLives, BML_INTEROP_FIELD_INT, startLives);
            if (status == BML_OK) status = Set(record, GameplayApi::EnergyStateField::TimeFactor, BML_INTEROP_FIELD_FLOAT, timeFactor);
            if (status == BML_OK) status = Set(record, GameplayApi::EnergyStateField::LifeBonus, BML_INTEROP_FIELD_INT, lifeBonus);
            return status;
        }
        return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
    }

    int ReadGameplayPage(BML_InteropPageBuilder *page, const BML_InteropProviderRequest *request) {
        ModContext *context = GetContext();
        if (!context)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        CKDataArray *array = nullptr;
        if (std::strcmp(request->Endpoint, "catalog") == 0)
            array = context->GetArrayByName("AllLevel");
        else if (std::strcmp(request->Endpoint, "checkpoints") == 0)
            array = context->GetArrayByName("Checkpoints");
        else if (std::strcmp(request->Endpoint, "resetpoints") == 0)
            array = context->GetArrayByName("ResetPoints");
        else
            return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
        if (!array || request->Offset > static_cast<uint64_t>((std::numeric_limits<int>::max)()))
            return BML_ERROR_INTEROP_UNSUPPORTED;
        const int first = static_cast<int>(request->Offset);
        const int rows = array->GetRowCount();
        const int maximum = request->Limit > static_cast<uint32_t>((std::numeric_limits<int>::max)())
                                ? (std::numeric_limits<int>::max)()
                                : static_cast<int>(request->Limit);
        const int end = (std::min)(rows, first > rows ? rows : first + maximum);
        for (int row = first; row < end; ++row) {
            BML_InteropRecordBuilder *record = context->GetInteropRegistry().PageAppend(page);
            if (!record)
                return BML_ERROR_OUT_OF_MEMORY;
            int status = BML_OK;
            if (std::strcmp(request->Endpoint, "catalog") == 0) {
                std::string file, startBall, sky;
                int bonus = 0, music = 0;
                if (!ReadString(array, row, 0, file) || !ReadString(array, row, 1, startBall) || !ReadString(array, row, 2, sky) ||
                    !ReadValue(array, row, 3, bonus) || !ReadValue(array, row, 4, music))
                    return BML_ERROR_INTEROP_UNSUPPORTED;
                status = SetString(record, GameplayApi::CatalogEntryField::File, file);
                if (status == BML_OK) status = SetString(record, GameplayApi::CatalogEntryField::StartBall, startBall);
                if (status == BML_OK) status = SetString(record, GameplayApi::CatalogEntryField::Sky, sky);
                if (status == BML_OK) status = Set(record, GameplayApi::CatalogEntryField::Bonus, BML_INTEROP_FIELD_INT, bonus);
                if (status == BML_OK) status = Set(record, GameplayApi::CatalogEntryField::Music, BML_INTEROP_FIELD_INT, music);
            } else if (std::strcmp(request->Endpoint, "checkpoints") == 0) {
                VxMatrix matrix;
                if (!ReadMatrix(array, row, 0, matrix))
                    return BML_ERROR_INTEROP_UNSUPPORTED;
                status = Set(record, GameplayApi::CheckpointField::Matrix, BML_INTEROP_FIELD_MAT4, BML::ToMat4(matrix));
                if (status == BML_OK) status = Set(record, GameplayApi::CheckpointField::Object, BML_INTEROP_FIELD_OBJECT,
                                                    m_ObjectReferences.Make(ReadObject(array, row, 1)));
            } else {
                status = Set(record, GameplayApi::ResetpointField::Object, BML_INTEROP_FIELD_OBJECT,
                             m_ObjectReferences.Make(ReadObject(array, row, 0)));
            }
            if (status != BML_OK)
                return status;
        }
        context->GetInteropRegistry().PageFinish(page, end >= rows ? 1 : 0);
        return BML_OK;
    }

    int ReadUiResource(BML_InteropRecordBuilder *record, const char *endpoint) {
        if (std::strcmp(endpoint, "state") != 0)
            return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
        int status = Set(record, UiApi::HudStateField::Mode, BML_INTEROP_FIELD_INT, m_Mod.GetHUD());
        if (status == BML_OK) status = Set(record, UiApi::HudStateField::SrTime, BML_INTEROP_FIELD_FLOAT, m_Mod.GetSRTime());
        return status;
    }

    int InvokeUi(BML_InteropRecordBuilder *, const BML_InteropProviderRequest *request) {
        if (!request->Input)
            return BML_ERROR_INTEROP_RECORD_INVALID;
        ModContext *context = GetContext();
        if (!context)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        const char *endpoint = request->Endpoint;
        if (std::strcmp(endpoint, "message_add") == 0) {
            std::string message;
            const int status = ReadViewString(request->Input, UiApi::MessageInputField::Message, message);
            if (status != BML_OK) return status;
            m_Mod.AddIngameMessage(message.c_str());
        } else if (std::strcmp(endpoint, "message_clear") == 0) {
            m_Mod.ClearIngameMessages();
        } else if (std::strcmp(endpoint, "mods_menu_open") == 0) {
            m_Mod.OpenModsMenu();
        } else if (std::strcmp(endpoint, "mods_menu_close") == 0) {
            m_Mod.CloseModsMenu();
        } else if (std::strcmp(endpoint, "map_menu_open") == 0) {
            m_Mod.OpenMapMenu();
        } else if (std::strcmp(endpoint, "map_menu_close") == 0) {
            m_Mod.CloseMapMenu();
        } else if (std::strcmp(endpoint, "hud_set") == 0) {
            int mode = 0;
            const int status = context->GetInteropRegistry().RecordViewGetInt(request->Input, UiApi::HudModeInputField::Mode, &mode);
            if (status != BML_OK) return status;
            m_Mod.SetHUD(mode);
        } else if (std::strcmp(endpoint, "hud_title_show") == 0 || std::strcmp(endpoint, "hud_fps_show") == 0 ||
                   std::strcmp(endpoint, "hud_sr_show") == 0) {
            int visible = 0;
            const int status = context->GetInteropRegistry().RecordViewGetBool(request->Input, UiApi::VisibleInputField::Visible, &visible);
            if (status != BML_OK) return status;
            if (std::strcmp(endpoint, "hud_title_show") == 0) m_Mod.ShowTitle(visible != 0);
            else if (std::strcmp(endpoint, "hud_fps_show") == 0) m_Mod.ShowFPS(visible != 0);
            else m_Mod.ShowSRTimer(visible != 0);
        } else if (std::strcmp(endpoint, "hud_sr_start") == 0) {
            m_Mod.StartSRTimer();
        } else if (std::strcmp(endpoint, "hud_sr_pause") == 0) {
            m_Mod.PauseSRTimer();
        } else if (std::strcmp(endpoint, "hud_sr_reset") == 0) {
            m_Mod.ResetSRTimer();
        } else {
            return BML_ERROR_INTEROP_ENDPOINT_NOT_FOUND;
        }
        return BML_OK;
    }

    int ReadViewString(const BML_InteropRecordView *view, uint32_t field, std::string &outValue) const {
        ModContext *context = GetContext();
        if (!context)
            return BML_ERROR_INTEROP_UNSUPPORTED;
        size_t required = 0;
        int status = context->GetInteropRegistry().RecordViewGetString(view, field, nullptr, 0, &required);
        if (status != BML_OK)
            return status;
        std::vector<char> storage(required ? required : 1);
        status = context->GetInteropRegistry().RecordViewGetString(view, field, storage.data(), storage.size(), &required);
        if (status == BML_OK)
            outValue.assign(storage.data(), required > 0 ? required - 1 : 0);
        return status;
    }

    static int ReadRuntimeResource(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record, void *userdata) {
        return request && record && userdata ? static_cast<BuiltinInteropProvider *>(userdata)->ReadRuntime(record, request->Endpoint)
                                            : BML_ERROR_INVALID_PARAMETER;
    }

    static int ReadSceneComponentCallback(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record, void *userdata) {
        return request && record && userdata ? static_cast<BuiltinInteropProvider *>(userdata)->ReadSceneComponent(record, request)
                                            : BML_ERROR_INVALID_PARAMETER;
    }

    static int InvokeSceneQuery(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record, void *userdata) {
        return request && record && userdata ? static_cast<BuiltinInteropProvider *>(userdata)->ReadSceneQuery(record, request)
                                            : BML_ERROR_INVALID_PARAMETER;
    }

    static int ProbeGameplay(const BML_InteropProviderRequest *request, void *userdata) {
        return request && userdata && static_cast<BuiltinInteropProvider *>(userdata)->ProbeGameplaySource(request->Endpoint)
                   ? BML_OK
                   : BML_ERROR_INTEROP_UNSUPPORTED;
    }

    static int ReadGameplayResourceCallback(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record, void *userdata) {
        return request && record && userdata ? static_cast<BuiltinInteropProvider *>(userdata)->ReadGameplayResource(record, request->Endpoint)
                                            : BML_ERROR_INVALID_PARAMETER;
    }

    static int ReadGameplayCollection(const BML_InteropProviderRequest *request, BML_InteropPageBuilder *page, void *userdata) {
        return request && page && userdata ? static_cast<BuiltinInteropProvider *>(userdata)->ReadGameplayPage(page, request)
                                          : BML_ERROR_INVALID_PARAMETER;
    }

    static int ReadUiResourceCallback(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record, void *userdata) {
        return request && record && userdata ? static_cast<BuiltinInteropProvider *>(userdata)->ReadUiResource(record, request->Endpoint)
                                            : BML_ERROR_INVALID_PARAMETER;
    }

    static int InvokeUiCommand(const BML_InteropProviderRequest *request, BML_InteropRecordBuilder *record, void *userdata) {
        return request && record && userdata ? static_cast<BuiltinInteropProvider *>(userdata)->InvokeUi(record, request)
                                            : BML_ERROR_INVALID_PARAMETER;
    }

    BMLMod &m_Mod;
    ObjectReferences m_ObjectReferences;
    ImcRuntimeApi::Provider m_ImcRuntime;
    ImcSceneApi::Provider m_ImcScene;
    ImcGameplayApi::Provider m_ImcGameplay;
    ImcUiApi::Provider m_ImcUi;
    ImcEventsApi::Client m_ImcEvents;
};

std::unordered_map<BMLMod *, std::unique_ptr<BuiltinInteropProvider>> g_Providers;

BuiltinInteropProvider *FindProvider(ModContext &context) {
    for (const auto &[mod, provider] : g_Providers) {
        (void)mod;
        if (provider && provider->Context() == &context)
            return provider.get();
    }
    return nullptr;
}

} // namespace

void RegisterBuiltinInteropApis(BMLMod &mod, ILogger *logger) {
    UnregisterBuiltinInteropApis(mod);
    auto provider = std::make_unique<BuiltinInteropProvider>(mod);
    const int status = provider->Register();
    if (status != BML_OK) {
        if (logger)
            logger->Warn("Failed to register built-in Interop registry: %s", BML_GetErrorString(status));
        return;
    }
    g_Providers.emplace(&mod, std::move(provider));
}

void UnregisterBuiltinInteropApis(BMLMod &mod) {
    const auto found = g_Providers.find(&mod);
    if (found == g_Providers.end())
        return;
    found->second->Unregister();
    g_Providers.erase(found);
}

void PublishBuiltinInteropEvent(ModContext &context, const BML::InteropEventSnapshot &event) {
    BuiltinInteropProvider *provider = FindProvider(context);
    if (!provider)
        return;
    try {
        provider->PublishEvent(event);
    } catch (...) {
        // Hook paths must never let a telemetry allocation or provider error
        // alter the original game callback.
    }
}

bool HasBuiltinInteropEventConsumers(ModContext &context) noexcept {
    try {
        BuiltinInteropProvider *provider = FindProvider(context);
        return (provider && provider->HasImcEventConsumers()) ||
               context.GetInteropRegistry().HasStreamConsumers(EventsApi::Descriptor.ApiId, "all");
    } catch (...) {
        return false;
    }
}

void InvalidateBuiltinObjectRefs(ModContext &context, const CK_ID *ids, int count) {
    if (BuiltinInteropProvider *provider = FindProvider(context))
        provider->InvalidateObjectRefs(ids, count);
}

void InvalidateAllBuiltinObjectRefs(ModContext &context) {
    if (BuiltinInteropProvider *provider = FindProvider(context))
        provider->InvalidateAllObjectRefs();
}

BML_ObjectRef MakeBuiltinObjectRef(ModContext &context, CKObject *object) {
    BuiltinInteropProvider *provider = FindProvider(context);
    return provider ? provider->MakeObjectRef(object) : BML_ObjectRef{};
}

CKObject *ResolveBuiltinObjectRef(ModContext &context, BML_ObjectRef reference) {
    BuiltinInteropProvider *provider = FindProvider(context);
    return provider ? provider->ResolveObjectRef(reference) : nullptr;
}
