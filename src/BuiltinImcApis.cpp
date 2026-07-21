#include "BuiltinImcApis.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BML/ImcMath.h"

#include "BMLMod.h"
#include "ImcEventSnapshot.h"
#include "ImcObjectReferenceRegistry.h"
#include "Logger.h"
#include "ModContext.h"
#include "BML/Generated/bml_events_imc.hpp"
#include "BML/Generated/bml_gameplay_imc.hpp"
#include "BML/Generated/bml_scene_imc.hpp"
#include "BML/Generated/bml_ui_imc.hpp"
#include "BML/Generated/bml_runtime_imc.hpp"

namespace {

namespace ImcRuntimeApi = BML::Imc::Generated::Bml::Runtime;
namespace ImcEventsApi = BML::Imc::Generated::Bml::Events;
namespace ImcGameplayApi = BML::Imc::Generated::Bml::Gameplay;
namespace ImcSceneApi = BML::Imc::Generated::Bml::Scene;
namespace ImcUiApi = BML::Imc::Generated::Bml::Ui;

constexpr uint32_t kVirtoolsObjectDomain = BML_IMC_OBJECT_DOMAIN_VIRTOOLS;

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
    ImcObjectReferenceRegistry m_References;
};

class BuiltinImcProvider {
public:
    explicit BuiltinImcProvider(BMLMod &mod) : m_Mod(mod) {}

    int Register() { return RegisterImc(); }

    void Unregister() {
        (void)m_ImcEvents.Close();
        (void)m_ImcUi.Close();
        (void)m_ImcGameplay.Close();
        (void)m_ImcScene.Close();
        (void)m_ImcRuntime.Close();
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

    void PublishEvent(const BML::ImcEventSnapshot &event) { PublishImcEvent(event); }

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
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context) return BML_ERROR_IMC_UNSUPPORTED;
        out.InGame = context->IsIngame(); out.InLevel = context->IsInLevel();
        out.Paused = context->IsPaused(); out.Playing = context->IsPlaying();
        out.CheatEnabled = context->IsCheatEnabled(); return BML_OK;
    }

    static int ReadImcRuntimeClock(ImcRuntimeApi::ClockStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        CKTimeManager *time = context ? context->GetTimeManager() : nullptr;
        if (!time) return BML_ERROR_IMC_UNSUPPORTED;
        out.TimeMs = time->GetTime(); out.AbsoluteMs = time->GetAbsoluteTime(); out.DeltaMs = time->GetLastDeltaTime();
        const CKDWORD tick = time->GetMainTickCount();
        out.Frame = tick > static_cast<CKDWORD>((std::numeric_limits<int>::max)())
                        ? (std::numeric_limits<int>::max)() : static_cast<int>(tick);
        return BML_OK;
    }

    static int ReadImcRuntimeScore(ImcRuntimeApi::ScoreStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        if (!provider) return BML_ERROR_INVALID_PARAMETER;
        out.Sr = provider->m_Mod.GetSRScore(); out.Hs = provider->m_Mod.GetHSScore(); return BML_OK;
    }

    void PublishImcEvent(const BML::ImcEventSnapshot &event) {
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
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        CKObject *object = provider ? provider->m_ObjectReferences.Resolve(provider->GetContext(), reference) : nullptr;
        if (!object) return BML_ERROR_IMC_OBJECT_INVALID;
        out.Id = static_cast<int>(object->GetID()); out.Name = object->GetName() ? object->GetName() : "";
        out.ClassId = static_cast<int>(object->GetClassID()); out.Visible = object->IsVisible() != FALSE;
        out.Dynamic = object->IsDynamic() != FALSE; return BML_OK;
    }

    static int ReadImcSceneEntity(const BML_ObjectRef &reference, ImcSceneApi::EntityTransformValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        CKObject *object = provider ? provider->m_ObjectReferences.Resolve(provider->GetContext(), reference) : nullptr;
        CK3dEntity *entity = dynamic_cast<CK3dEntity *>(object);
        if (!entity) return BML_ERROR_IMC_OBJECT_INVALID;
        VxVector position, scale; entity->GetPosition(&position); entity->GetScale(&scale);
        out.Position = BML::Imc::ToVec3(position); out.Scale = BML::Imc::ToVec3(scale);
        out.Parent = provider->m_ObjectReferences.Make(entity->GetParent());
        out.ChildCount = entity->GetChildrenCount(); return BML_OK;
    }

    static int ReadImcSceneFindName(const ImcSceneApi::FindNameRequestValue &input,
                                    ImcSceneApi::FindResultValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !context->GetCKContext()) return BML_ERROR_IMC_UNSUPPORTED;
        out.Object = provider->m_ObjectReferences.Make(context->GetCKContext()->GetObjectByName(const_cast<char *>(input.Name.c_str())));
        return BML_OK;
    }

    static int ReadImcSceneFindNameClass(const ImcSceneApi::FindNameClassRequestValue &input,
                                         ImcSceneApi::FindResultValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !context->GetCKContext()) return BML_ERROR_IMC_UNSUPPORTED;
        out.Object = provider->m_ObjectReferences.Make(
            context->GetCKContext()->GetObjectByNameAndClass(const_cast<char *>(input.Name.c_str()), static_cast<CK_CLASSID>(input.ClassId)));
        return BML_OK;
    }

    static int ReadImcGameplayLevel(ImcGameplayApi::LevelStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("level")) return BML_ERROR_IMC_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("CurrentLevel");
        int id = 0, points = 0; VxMatrix matrix;
        if (!ReadValue(array, 0, 0, id) || !ReadMatrix(array, 0, 3, matrix) || !ReadValue(array, 0, 5, points))
            return BML_ERROR_IMC_UNSUPPORTED;
        out.Id = id; out.ActiveBall = provider->m_ObjectReferences.Make(ReadObject(array, 0, 1));
        out.ResetMatrix = BML::Imc::ToMat4(matrix); out.Points = points; return BML_OK;
    }

    static int ReadImcGameplayEnergy(ImcGameplayApi::EnergyStateValue &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("energy")) return BML_ERROR_IMC_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("Energy");
        if (!ReadValue(array, 0, 0, out.Points) || !ReadValue(array, 0, 1, out.Lives) ||
            !ReadValue(array, 0, 2, out.StartPoints) || !ReadValue(array, 0, 3, out.StartLives) ||
            !ReadValue(array, 0, 4, out.TimeFactor) || !ReadValue(array, 0, 5, out.LifeBonus))
            return BML_ERROR_IMC_UNSUPPORTED;
        return BML_OK;
    }

    static int ReadImcGameplayCatalog(std::vector<ImcGameplayApi::CatalogEntryValue> &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("catalog")) return BML_ERROR_IMC_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("AllLevel"); out.clear(); out.reserve(array->GetRowCount());
        for (int row = 0; row < array->GetRowCount(); ++row) {
            ImcGameplayApi::CatalogEntryValue value{};
            if (!ReadString(array, row, 0, value.File) || !ReadString(array, row, 1, value.StartBall) ||
                !ReadString(array, row, 2, value.Sky) || !ReadValue(array, row, 3, value.Bonus) ||
                !ReadValue(array, row, 4, value.Music)) return BML_ERROR_IMC_UNSUPPORTED;
            out.push_back(std::move(value));
        }
        return BML_OK;
    }

    static int ReadImcGameplayCheckpoints(std::vector<ImcGameplayApi::CheckpointValue> &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("checkpoints")) return BML_ERROR_IMC_UNSUPPORTED;
        CKDataArray *array = context->GetArrayByName("Checkpoints"); out.clear(); out.reserve(array->GetRowCount());
        for (int row = 0; row < array->GetRowCount(); ++row) {
            VxMatrix matrix; if (!ReadMatrix(array, row, 0, matrix)) return BML_ERROR_IMC_UNSUPPORTED;
            ImcGameplayApi::CheckpointValue value{}; value.Matrix = BML::Imc::ToMat4(matrix);
            value.Object = provider->m_ObjectReferences.Make(ReadObject(array, row, 1)); out.push_back(value);
        }
        return BML_OK;
    }

    static int ReadImcGameplayResetpoints(std::vector<ImcGameplayApi::ResetpointValue> &out, void *userdata) {
        auto *provider = static_cast<BuiltinImcProvider *>(userdata);
        ModContext *context = provider ? provider->GetContext() : nullptr;
        if (!context || !provider->ProbeGameplaySource("resetpoints")) return BML_ERROR_IMC_UNSUPPORTED;
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

    static BuiltinImcProvider *ImcUiProvider(void *userdata) {
        return static_cast<BuiltinImcProvider *>(userdata);
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

    BMLMod &m_Mod;
    ObjectReferences m_ObjectReferences;
    ImcRuntimeApi::Provider m_ImcRuntime;
    ImcSceneApi::Provider m_ImcScene;
    ImcGameplayApi::Provider m_ImcGameplay;
    ImcUiApi::Provider m_ImcUi;
    ImcEventsApi::Client m_ImcEvents;
};

std::unordered_map<BMLMod *, std::unique_ptr<BuiltinImcProvider>> g_Providers;

BuiltinImcProvider *FindProvider(ModContext &context) {
    for (const auto &[mod, provider] : g_Providers) {
        (void)mod;
        if (provider && provider->Context() == &context)
            return provider.get();
    }
    return nullptr;
}

} // namespace

void RegisterBuiltinImcApis(BMLMod &mod, ILogger *logger) {
    UnregisterBuiltinImcApis(mod);
    auto provider = std::make_unique<BuiltinImcProvider>(mod);
    const int status = provider->Register();
    if (status != BML_OK) {
        if (logger)
            logger->Warn("Failed to register built-in IMC providers: %s", BML_GetErrorString(status));
        return;
    }
    g_Providers.emplace(&mod, std::move(provider));
}

void UnregisterBuiltinImcApis(BMLMod &mod) {
    const auto found = g_Providers.find(&mod);
    if (found == g_Providers.end())
        return;
    found->second->Unregister();
    g_Providers.erase(found);
}

void PublishBuiltinImcEvent(ModContext &context, const BML::ImcEventSnapshot &event) {
    BuiltinImcProvider *provider = FindProvider(context);
    if (!provider)
        return;
    try {
        provider->PublishEvent(event);
    } catch (...) {
        // Hook paths must never let a telemetry allocation or provider error
        // alter the original game callback.
    }
}

bool HasBuiltinImcEventConsumers(ModContext &context) noexcept {
    try {
        BuiltinImcProvider *provider = FindProvider(context);
        return provider && provider->HasImcEventConsumers();
    } catch (...) {
        return false;
    }
}

void InvalidateBuiltinObjectRefs(ModContext &context, const CK_ID *ids, int count) {
    if (BuiltinImcProvider *provider = FindProvider(context))
        provider->InvalidateObjectRefs(ids, count);
}

void InvalidateAllBuiltinObjectRefs(ModContext &context) {
    if (BuiltinImcProvider *provider = FindProvider(context))
        provider->InvalidateAllObjectRefs();
}

BML_ObjectRef MakeBuiltinObjectRef(ModContext &context, CKObject *object) {
    BuiltinImcProvider *provider = FindProvider(context);
    return provider ? provider->MakeObjectRef(object) : BML_ObjectRef{};
}

CKObject *ResolveBuiltinObjectRef(ModContext &context, BML_ObjectRef reference) {
    BuiltinImcProvider *provider = FindProvider(context);
    return provider ? provider->ResolveObjectRef(reference) : nullptr;
}
