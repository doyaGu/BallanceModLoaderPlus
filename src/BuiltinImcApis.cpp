#include "BuiltinImcApis.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "BML/Gameplay.h"
#include "BML/ImcMath.h"
#include "BML/Scene.h"

#include "BMLMod.h"
#include "ImcEventSnapshot.h"
#include "ImcObjectReferenceRegistry.h"
#include "Logger.h"
#include "ModContext.h"
#include "BML/Generated/bml_events_imc.hpp"

namespace {

namespace ImcEventsApi = BML::Imc::Generated::Bml::Events;

constexpr uint32_t kVirtoolsObjectDomain = BML_OBJECT_DOMAIN_VIRTOOLS;

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

    int ReadSceneObject(BML_ObjectRef reference, BML_SceneObjectInfo &out) {
        CKObject *object = m_ObjectReferences.Resolve(GetContext(), reference);
        if (!object)
            return BML_ERROR_OBJECT_INVALID;
        out.Id = static_cast<int>(object->GetID());
        out.ClassId = static_cast<int>(object->GetClassID());
        WriteText(out.Name, out.NameLength, object->GetName());
        out.Visible = object->IsVisible() != FALSE ? 1 : 0;
        out.Dynamic = object->IsDynamic() != FALSE ? 1 : 0;
        return BML_OK;
    }

    int ReadSceneEntityTransform(BML_ObjectRef reference, BML_SceneEntityTransform &out) {
        auto *entity = dynamic_cast<CK3dEntity *>(m_ObjectReferences.Resolve(GetContext(), reference));
        if (!entity)
            return BML_ERROR_OBJECT_INVALID;
        VxVector position, scale;
        entity->GetPosition(&position);
        entity->GetScale(&scale);
        out.Position = BML::Imc::ToVec3(position);
        out.Scale = BML::Imc::ToVec3(scale);
        out.Parent = m_ObjectReferences.Make(entity->GetParent());
        out.ChildCount = entity->GetChildrenCount();
        return BML_OK;
    }

    int FindSceneObject(const char *name, BML_ObjectRef &out) {
        ModContext *context = GetContext();
        if (!context || !context->GetCKContext())
            return BML_ERROR_UNAVAILABLE;
        out = m_ObjectReferences.Make(context->GetCKContext()->GetObjectByName(const_cast<char *>(name)));
        return BML_OK;
    }

    int FindSceneObjectOfClass(const char *name, int classId, BML_ObjectRef &out) {
        ModContext *context = GetContext();
        if (!context || !context->GetCKContext())
            return BML_ERROR_UNAVAILABLE;
        out = m_ObjectReferences.Make(context->GetCKContext()->GetObjectByNameAndClass(
            const_cast<char *>(name), static_cast<CK_CLASSID>(classId)));
        return BML_OK;
    }

    int ReadGameplayLevel(BML_GameplayLevelState &out) {
        CKDataArray *array = GameplaySource("level", "CurrentLevel");
        if (!array)
            return BML_ERROR_UNAVAILABLE;
        int id = 0, points = 0;
        VxMatrix matrix;
        if (!ReadValue(array, 0, 0, id) || !ReadMatrix(array, 0, 3, matrix) ||
            !ReadValue(array, 0, 5, points))
            return BML_ERROR_UNAVAILABLE;
        out.Id = id;
        out.ActiveBall = m_ObjectReferences.Make(ReadObject(array, 0, 1));
        out.ResetMatrix = BML::Imc::ToMat4(matrix);
        out.Points = points;
        return BML_OK;
    }

    int ReadGameplayEnergy(BML_GameplayEnergyState &out) {
        CKDataArray *array = GameplaySource("energy", "Energy");
        if (!array)
            return BML_ERROR_UNAVAILABLE;
        if (!ReadValue(array, 0, 0, out.Points) || !ReadValue(array, 0, 1, out.Lives) ||
            !ReadValue(array, 0, 2, out.StartPoints) || !ReadValue(array, 0, 3, out.StartLives) ||
            !ReadValue(array, 0, 4, out.TimeFactor) || !ReadValue(array, 0, 5, out.LifeBonus))
            return BML_ERROR_UNAVAILABLE;
        return BML_OK;
    }

    int ReadGameplayCatalogCount(std::size_t &out) {
        return CountGameplayRows("catalog", "AllLevel", out);
    }

    int ReadGameplayCatalogEntry(std::size_t index, BML_GameplayCatalogEntry &out) {
        CKDataArray *array = GameplaySource("catalog", "AllLevel");
        if (!array)
            return BML_ERROR_UNAVAILABLE;
        const int row = RowIndex(array, index);
        if (row < 0)
            return BML_ERROR_NOT_FOUND;
        std::string file, startBall, sky;
        int bonus = 0, music = 0;
        if (!ReadString(array, row, 0, file) || !ReadString(array, row, 1, startBall) ||
            !ReadString(array, row, 3, sky) || !ReadValue(array, row, 6, bonus) ||
            !ReadValue(array, row, 7, music))
            return BML_ERROR_UNAVAILABLE;
        WriteText(out.File, out.FileLength, file.c_str());
        WriteText(out.StartBall, out.StartBallLength, startBall.c_str());
        WriteText(out.Sky, out.SkyLength, sky.c_str());
        out.Bonus = bonus;
        out.Music = music;
        return BML_OK;
    }

    int ReadGameplayCheckpointCount(std::size_t &out) {
        return CountGameplayRows("checkpoints", "Checkpoints", out);
    }

    int ReadGameplayCheckpoint(std::size_t index, BML_GameplayCheckpoint &out) {
        CKDataArray *array = GameplaySource("checkpoints", "Checkpoints");
        if (!array)
            return BML_ERROR_UNAVAILABLE;
        const int row = RowIndex(array, index);
        if (row < 0)
            return BML_ERROR_NOT_FOUND;
        VxMatrix matrix;
        if (!ReadMatrix(array, row, 0, matrix))
            return BML_ERROR_UNAVAILABLE;
        out.Matrix = BML::Imc::ToMat4(matrix);
        out.Object = m_ObjectReferences.Make(ReadObject(array, row, 1));
        return BML_OK;
    }

    int ReadGameplayResetpointCount(std::size_t &out) {
        return CountGameplayRows("resetpoints", "ResetPoints", out);
    }

    int ReadGameplayResetpoint(std::size_t index, BML_GameplayResetpoint &out) {
        CKDataArray *array = GameplaySource("resetpoints", "ResetPoints");
        if (!array)
            return BML_ERROR_UNAVAILABLE;
        const int row = RowIndex(array, index);
        if (row < 0)
            return BML_ERROR_NOT_FOUND;
        out.Object = m_ObjectReferences.Make(ReadObject(array, row, 0));
        return BML_OK;
    }

private:
    int RegisterImc() {
        const int status = m_ImcEvents.Open(m_Mod.GetID());
        if (status != BML_OK)
            (void)m_ImcEvents.Close();
        return status;
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

    ModContext *GetContext() const { return m_Mod.GetRuntimeContext(); }

    // outLength is the whole length, so text too long for the buffer is detectable
    // instead of silently short.
    template <std::size_t Capacity>
    static void WriteText(char (&buffer)[Capacity], int &outLength, const char *text) {
        const std::size_t length = text ? std::strlen(text) : 0u;
        const std::size_t copied = (std::min)(length, Capacity - 1u);
        if (copied != 0u)
            std::memcpy(buffer, text, copied);
        buffer[copied] = 0;
        outLength = static_cast<int>(length);
    }

    // The array a gameplay read works on, or null when there is nothing to read:
    // no context yet, the array missing because no level is loaded, or a column
    // layout that is not the one the readers assume.
    CKDataArray *GameplaySource(const char *endpoint, const char *arrayName) const {
        ModContext *context = GetContext();
        if (!context || !ProbeGameplaySource(endpoint))
            return nullptr;
        return context->GetArrayByName(arrayName);
    }

    int CountGameplayRows(const char *endpoint, const char *arrayName, std::size_t &out) const {
        CKDataArray *array = GameplaySource(endpoint, arrayName);
        if (!array)
            return BML_ERROR_UNAVAILABLE;
        const int rows = array->GetRowCount();
        out = rows > 0 ? static_cast<std::size_t>(rows) : 0u;
        return BML_OK;
    }

    // A row index is checked against the array as it is right now, so an index
    // that was inside it before a level change is simply not there any more.
    static int RowIndex(CKDataArray *array, std::size_t index) {
        const int rows = array->GetRowCount();
        return rows > 0 && index < static_cast<std::size_t>(rows) ? static_cast<int>(index) : -1;
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
                              {"Levelfile", "StartBall", "StartResetpoint", "Sky", "Light",
                               "Skytranslation", "LevelBonus", "Music"});
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

// Every interface thunk and every script binding reaches the provider through
// here.  The catalog read allocates, so the catch is what keeps a bad_alloc from
// crossing back out to a Mod or to a script.
template <typename Reader, typename... Args>
int ServeBuiltinProvider(ModContext &context, Reader reader, Args &&...args) {
    BuiltinImcProvider *provider = FindProvider(context);
    if (!provider)
        return BML_ERROR_UNAVAILABLE;
    try {
        return (provider->*reader)(std::forward<Args>(args)...);
    } catch (const std::bad_alloc &) {
        return BML_ERROR_OUT_OF_MEMORY;
    } catch (...) {
        return BML_ERROR_FAIL;
    }
}

int ReadBuiltinSceneObject(ModContext &context, BML_ObjectRef object, BML_SceneObjectInfo &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadSceneObject, object, out);
}

int ReadBuiltinSceneEntityTransform(ModContext &context, BML_ObjectRef object,
                                    BML_SceneEntityTransform &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadSceneEntityTransform, object, out);
}

int FindBuiltinSceneObject(ModContext &context, const char *name, BML_ObjectRef &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::FindSceneObject, name, out);
}

int FindBuiltinSceneObjectOfClass(ModContext &context, const char *name, int classId,
                                  BML_ObjectRef &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::FindSceneObjectOfClass, name, classId,
                                out);
}

int ReadBuiltinGameplayLevel(ModContext &context, BML_GameplayLevelState &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadGameplayLevel, out);
}

int ReadBuiltinGameplayEnergy(ModContext &context, BML_GameplayEnergyState &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadGameplayEnergy, out);
}

int ReadBuiltinGameplayCatalogCount(ModContext &context, std::size_t &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadGameplayCatalogCount, out);
}

int ReadBuiltinGameplayCatalogEntry(ModContext &context, std::size_t index,
                                    BML_GameplayCatalogEntry &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadGameplayCatalogEntry, index, out);
}

int ReadBuiltinGameplayCheckpointCount(ModContext &context, std::size_t &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadGameplayCheckpointCount, out);
}

int ReadBuiltinGameplayCheckpoint(ModContext &context, std::size_t index,
                                  BML_GameplayCheckpoint &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadGameplayCheckpoint, index, out);
}

int ReadBuiltinGameplayResetpointCount(ModContext &context, std::size_t &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadGameplayResetpointCount, out);
}

int ReadBuiltinGameplayResetpoint(ModContext &context, std::size_t index,
                                  BML_GameplayResetpoint &out) {
    return ServeBuiltinProvider(context, &BuiltinImcProvider::ReadGameplayResetpoint, index, out);
}
