#include "ScriptBuiltinFacade.h"

#include <cstddef>
#include <limits>
#include <new>
#include <string>

#include <angelscript.h>

#include "BML/Gameplay.h"

#include "Api/BuiltinCapabilities.h"
#include "Loader/ModContext.h"
#include "ScriptMod.h"
#include "ScriptModRuntime.h"
#include "ScriptFunctionSupport.h"
#include "ScriptStringInterop.h"

namespace {

struct RuntimeState {
    bool InGame = false;
    bool InLevel = false;
    bool Paused = false;
    bool Playing = false;
    bool CheatEnabled = false;
};

struct ClockState {
    float TimeMs = 0.0f;
    float AbsoluteMs = 0.0f;
    float DeltaMs = 0.0f;
    int Frame = 0;
};

struct ScoreState {
    float SR = 0.0f;
    int HS = 0;
};

struct LevelState {
    int Id = 0;
    BML_ObjectRef ActiveBall{};
    BML_Mat4 ResetMatrix{};
    int Points = 0;
};

struct EnergyState {
    int Points = 0;
    int Lives = 0;
    int StartPoints = 0;
    int StartLives = 0;
    float TimeFactor = 0.0f;
    int LifeBonus = 0;
};

struct CatalogEntry {
    std::string File;
    std::string StartBall;
    std::string Sky;
    int Bonus = 0;
    int Music = 0;
    bool FileTruncated = false;
    bool StartBallTruncated = false;
    bool SkyTruncated = false;
};

struct Checkpoint {
    BML_Mat4 Matrix{};
    BML_ObjectRef Object{};
};

struct Resetpoint {
    BML_ObjectRef Object{};
};

std::string g_FacadeRegistrationError;

template <typename T>
void ConstructValue(T *self) {
    new (self) T();
}

template <typename T>
void CopyConstructValue(const T &other, T *self) {
    new (self) T(other);
}

template <typename T>
void DestructValue(T *self) {
    self->~T();
}

template <typename T>
T &AssignValue(const T &other, T *self) {
    *self = other;
    return *self;
}

#define BML_AS_DEFINE_IMC_VALUE(Type, Suffix)                                     \
    static void Construct##Suffix(Type *self) { ConstructValue(self); }            \
    static void CopyConstruct##Suffix(const Type &other, Type *self) {              \
        CopyConstructValue(other, self);                                            \
    }                                                                                \
    static void Destruct##Suffix(Type *self) { DestructValue(self); }               \
    static Type &Assign##Suffix(const Type &other, Type *self) {                    \
        return AssignValue(other, self);                                            \
    }

BML_AS_DEFINE_IMC_VALUE(RuntimeState, RuntimeState)
BML_AS_DEFINE_IMC_VALUE(ClockState, ClockState)
BML_AS_DEFINE_IMC_VALUE(ScoreState, ScoreState)
BML_AS_DEFINE_IMC_VALUE(LevelState, LevelState)
BML_AS_DEFINE_IMC_VALUE(EnergyState, EnergyState)
BML_AS_DEFINE_IMC_VALUE(CatalogEntry, CatalogEntry)
BML_AS_DEFINE_IMC_VALUE(Checkpoint, Checkpoint)
BML_AS_DEFINE_IMC_VALUE(Resetpoint, Resetpoint)

#undef BML_AS_DEFINE_IMC_VALUE

struct ValueTypeRegistration {
    const char *Name;
    int Size;
    asDWORD Flags;
    asSFuncPtr Construct;
    asSFuncPtr CopyConstruct;
    asSFuncPtr Destruct;
    asSFuncPtr Assign;
};

template <typename T>
ValueTypeRegistration ValueType(const char *name,
                                asSFuncPtr construct,
                                asSFuncPtr copyConstruct,
                                asSFuncPtr destruct,
                                asSFuncPtr assign) {
    return {name, sizeof(T), asOBJ_VALUE | asGetTypeTraits<T>(), construct, copyConstruct, destruct, assign};
}

bool Register(asIScriptEngine *engine, int status, const char *declaration, const char **errorMessage) {
    if (status >= 0)
        return true;
    g_FacadeRegistrationError = "Failed to register built-in facade declaration: ";
    g_FacadeRegistrationError += declaration;
    g_FacadeRegistrationError += " returned ";
    g_FacadeRegistrationError += std::to_string(status);
    if (errorMessage)
        *errorMessage = g_FacadeRegistrationError.c_str();
    if (engine)
        engine->SetDefaultNamespace("");
    return false;
}

bool RegisterValue(asIScriptEngine *engine, const ValueTypeRegistration &registration, const char **errorMessage) {
    const std::string copyConstructor = std::string("void f(const ") + registration.Name + " &in)";
    const std::string assign = std::string(registration.Name) + " &opAssign(const " + registration.Name + " &in)";
    return Register(engine,
                    engine->RegisterObjectType(registration.Name, registration.Size, registration.Flags),
                    registration.Name,
                    errorMessage) &&
           Register(engine,
                    engine->RegisterObjectBehaviour(registration.Name,
                                                    asBEHAVE_CONSTRUCT,
                                                    "void f()",
                                                    registration.Construct,
                                                    asCALL_CDECL_OBJLAST),
                    "value default constructor",
                    errorMessage) &&
           Register(engine,
                    engine->RegisterObjectBehaviour(registration.Name,
                                                    asBEHAVE_CONSTRUCT,
                                                    copyConstructor.c_str(),
                                                    registration.CopyConstruct,
                                                    asCALL_CDECL_OBJLAST),
                    "value copy constructor",
                    errorMessage) &&
           Register(engine,
                    engine->RegisterObjectBehaviour(registration.Name,
                                                    asBEHAVE_DESTRUCT,
                                                    "void f()",
                                                    registration.Destruct,
                                                    asCALL_CDECL_OBJLAST),
                    "value destructor",
                    errorMessage) &&
           Register(engine,
                    engine->RegisterObjectMethod(registration.Name,
                                                 assign.c_str(),
                                                 registration.Assign,
                                                 asCALL_CDECL_OBJLAST),
                    "value assignment",
                    errorMessage);
}

ModContext *GetCurrentModContext() {
    BML::ScriptMod *mod = BML::ScriptModRuntime::GetCurrentScriptMod();
    return mod ? mod->GetModContext() : nullptr;
}

int ReadCurrentModContext(ModContext *&outContext) {
    outContext = GetCurrentModContext();
    return outContext ? BML_OK : BML_ERROR_UNAVAILABLE;
}

ModContext *RequireRuntimeContext() {
    ModContext *context = GetCurrentModContext();
    if (context)
        return context;
    BML::ScriptStringInterop::RaiseActiveException(
        "BML::Runtime requires an active script mod callback.");
    return nullptr;
}

RuntimeState GetRuntimeState() {
    ModContext *context = RequireRuntimeContext();
    if (!context)
        return {};
    const BML::GameSessionSnapshot session = context->ReadGameSession();
    return {session.IsInGame(), session.IsInLevel(), session.IsPaused(), session.IsPlaying(),
            context->IsCheatEnabled()};
}

ClockState GetRuntimeClock() {
    ModContext *context = RequireRuntimeContext();
    CKTimeManager *time = context ? context->GetTimeManager() : nullptr;
    if (!time) {
        if (context) {
            BML::ScriptStringInterop::RaiseActiveException(
                "BML::Runtime clock is unavailable.");
        }
        return {};
    }
    const CKDWORD tick = time->GetMainTickCount();
    const CKDWORD maxFrame = static_cast<CKDWORD>((std::numeric_limits<int>::max)());
    return {time->GetTime(), time->GetAbsoluteTime(), time->GetLastDeltaTime(),
            tick > maxFrame ? (std::numeric_limits<int>::max)()
                            : static_cast<int>(tick)};
}

ScoreState GetRuntimeScore() {
    ModContext *context = RequireRuntimeContext();
    return context ? ScoreState{context->GetSRScore(), context->GetHSScore()}
                   : ScoreState{};
}

int ReadLevel(LevelState &out) {
    ModContext *context = nullptr;
    int status = ReadCurrentModContext(context);
    BML_GameplayLevelState value = {};
    if (status == BML_OK) status = ReadBuiltinGameplayLevel(*context, value);
    if (status == BML_OK)
        out = {value.Id, value.ActiveBall, value.ResetMatrix, value.Points};
    return status;
}

int ReadEnergy(EnergyState &out) {
    ModContext *context = nullptr;
    int status = ReadCurrentModContext(context);
    BML_GameplayEnergyState value = {};
    if (status == BML_OK) status = ReadBuiltinGameplayEnergy(*context, value);
    if (status == BML_OK) {
        out = {value.Points, value.Lives, value.StartPoints, value.StartLives,
               value.TimeFactor, value.LifeBonus};
    }
    return status;
}

CKObject *ResolveScriptObject(const BML_ObjectRef &reference) {
    ModContext *context = GetCurrentModContext();
    return context && reference.Domain != 0
               ? context->ObjectRefs().Resolve(reference)
               : nullptr;
}

CKObject *BorrowActiveBall(const LevelState *value) {
    return value ? ResolveScriptObject(value->ActiveBall) : nullptr;
}

CKObject *BorrowCheckpointObject(const Checkpoint *value) {
    return value ? ResolveScriptObject(value->Object) : nullptr;
}

CKObject *BorrowResetpointObject(const Resetpoint *value) {
    return value ? ResolveScriptObject(value->Object) : nullptr;
}

using GameplayCountReader = int (*)(ModContext &, std::size_t &);

int ReadGameplayCount(GameplayCountReader read, int &out) {
    ModContext *context = nullptr;
    int status = ReadCurrentModContext(context);
    std::size_t count = 0;
    if (status == BML_OK)
        status = read(*context, count);
    if (status == BML_OK) {
        if (count > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
            return BML_ERROR_FAIL;
        out = static_cast<int>(count);
    }
    return status;
}

int ReadCatalogCount(int &out) {
    return ReadGameplayCount(&ReadBuiltinGameplayCatalogCount, out);
}

int ReadCheckpointCount(int &out) {
    return ReadGameplayCount(&ReadBuiltinGameplayCheckpointCount, out);
}

int ReadResetpointCount(int &out) {
    return ReadGameplayCount(&ReadBuiltinGameplayResetpointCount, out);
}

int GetGameplayEntryContext(int index, ModContext *&outContext) {
    if (index < 0)
        return BML_ERROR_INVALID_PARAMETER;
    return ReadCurrentModContext(outContext);
}

int ReadCatalogEntry(int index, CatalogEntry &out) {
    ModContext *context = nullptr;
    int status = GetGameplayEntryContext(index, context);
    BML_GameplayCatalogEntry row = {};
    if (status == BML_OK)
        status = ReadBuiltinGameplayCatalogEntry(
            *context, static_cast<std::size_t>(index), row);
    if (status == BML_OK) {
        out = {
            std::string(row.File),
            std::string(row.StartBall),
            std::string(row.Sky),
            row.Bonus,
            row.Music,
            row.FileLength >= static_cast<int>(BML_GAMEPLAY_NAME_CAPACITY),
            row.StartBallLength >= static_cast<int>(BML_GAMEPLAY_NAME_CAPACITY),
            row.SkyLength >= static_cast<int>(BML_GAMEPLAY_NAME_CAPACITY),
        };
    }
    return status;
}

int ReadCheckpoint(int index, Checkpoint &out) {
    ModContext *context = nullptr;
    int status = GetGameplayEntryContext(index, context);
    BML_GameplayCheckpoint row = {};
    if (status == BML_OK)
        status = ReadBuiltinGameplayCheckpoint(
            *context, static_cast<std::size_t>(index), row);
    if (status == BML_OK)
        out = {row.Matrix, row.Object};
    return status;
}

int ReadResetpoint(int index, Resetpoint &out) {
    ModContext *context = nullptr;
    int status = GetGameplayEntryContext(index, context);
    BML_GameplayResetpoint row = {};
    if (status == BML_OK)
        status = ReadBuiltinGameplayResetpoint(
            *context, static_cast<std::size_t>(index), row);
    if (status == BML_OK)
        out = {row.Object};
    return status;
}

bool RegisterRuntime(asIScriptEngine *engine, const char **errorMessage) {
    if (!Register(engine, engine->SetDefaultNamespace("BML::Runtime"), "namespace BML::Runtime", errorMessage))
        return false;
    const ValueTypeRegistration values[] = {
        ValueType<RuntimeState>("State", asFUNCTION(ConstructRuntimeState), asFUNCTION(CopyConstructRuntimeState), asFUNCTION(DestructRuntimeState), asFUNCTION(AssignRuntimeState)),
        ValueType<ClockState>("Clock", asFUNCTION(ConstructClockState), asFUNCTION(CopyConstructClockState), asFUNCTION(DestructClockState), asFUNCTION(AssignClockState)),
        ValueType<ScoreState>("Score", asFUNCTION(ConstructScoreState), asFUNCTION(CopyConstructScoreState), asFUNCTION(DestructScoreState), asFUNCTION(AssignScoreState)),
    };
    for (const ValueTypeRegistration &value : values) {
        if (!RegisterValue(engine, value, errorMessage))
            return false;
    }
#define BML_AS_PROPERTY(Type, Declaration, Field) \
    if (!Register(engine, engine->RegisterObjectProperty(Type, Declaration, Field), Declaration, errorMessage)) return false
    BML_AS_PROPERTY("State", "bool InGame", asOFFSET(RuntimeState, InGame));
    BML_AS_PROPERTY("State", "bool InLevel", asOFFSET(RuntimeState, InLevel));
    BML_AS_PROPERTY("State", "bool Paused", asOFFSET(RuntimeState, Paused));
    BML_AS_PROPERTY("State", "bool Playing", asOFFSET(RuntimeState, Playing));
    BML_AS_PROPERTY("State", "bool CheatEnabled", asOFFSET(RuntimeState, CheatEnabled));
    BML_AS_PROPERTY("Clock", "float TimeMs", asOFFSET(ClockState, TimeMs));
    BML_AS_PROPERTY("Clock", "float AbsoluteMs", asOFFSET(ClockState, AbsoluteMs));
    BML_AS_PROPERTY("Clock", "float DeltaMs", asOFFSET(ClockState, DeltaMs));
    BML_AS_PROPERTY("Clock", "int Frame", asOFFSET(ClockState, Frame));
    BML_AS_PROPERTY("Score", "float SR", asOFFSET(ScoreState, SR));
    BML_AS_PROPERTY("Score", "int HS", asOFFSET(ScoreState, HS));
#undef BML_AS_PROPERTY
    return Register(engine, engine->RegisterGlobalFunction("State GetState()", BML_AS_GENERIC_FUNCTION(&GetRuntimeState), asCALL_GENERIC), "Runtime::GetState", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("Clock GetClock()", BML_AS_GENERIC_FUNCTION(&GetRuntimeClock), asCALL_GENERIC), "Runtime::GetClock", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("Score GetScore()", BML_AS_GENERIC_FUNCTION(&GetRuntimeScore), asCALL_GENERIC), "Runtime::GetScore", errorMessage) &&
           Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage);
}

bool RegisterGameplay(asIScriptEngine *engine, const char **errorMessage) {
    if (!Register(engine, engine->SetDefaultNamespace("BML::Gameplay"), "namespace BML::Gameplay", errorMessage))
        return false;
    const ValueTypeRegistration values[] = {
        ValueType<LevelState>("LevelState", asFUNCTION(ConstructLevelState), asFUNCTION(CopyConstructLevelState), asFUNCTION(DestructLevelState), asFUNCTION(AssignLevelState)),
        ValueType<EnergyState>("EnergyState", asFUNCTION(ConstructEnergyState), asFUNCTION(CopyConstructEnergyState), asFUNCTION(DestructEnergyState), asFUNCTION(AssignEnergyState)),
        ValueType<CatalogEntry>("CatalogEntry", asFUNCTION(ConstructCatalogEntry), asFUNCTION(CopyConstructCatalogEntry), asFUNCTION(DestructCatalogEntry), asFUNCTION(AssignCatalogEntry)),
        ValueType<Checkpoint>("Checkpoint", asFUNCTION(ConstructCheckpoint), asFUNCTION(CopyConstructCheckpoint), asFUNCTION(DestructCheckpoint), asFUNCTION(AssignCheckpoint)),
        ValueType<Resetpoint>("Resetpoint", asFUNCTION(ConstructResetpoint), asFUNCTION(CopyConstructResetpoint), asFUNCTION(DestructResetpoint), asFUNCTION(AssignResetpoint)),
    };
    for (const ValueTypeRegistration &value : values) {
        if (!RegisterValue(engine, value, errorMessage))
            return false;
    }
#define BML_AS_PROPERTY(Type, Declaration, Field) \
    if (!Register(engine, engine->RegisterObjectProperty(Type, Declaration, Field), Declaration, errorMessage)) return false
    BML_AS_PROPERTY("LevelState", "int Id", asOFFSET(LevelState, Id));
    BML_AS_PROPERTY("LevelState", "BML::Mat4 ResetMatrix", asOFFSET(LevelState, ResetMatrix));
    BML_AS_PROPERTY("LevelState", "int Points", asOFFSET(LevelState, Points));
    BML_AS_PROPERTY("EnergyState", "int Points", asOFFSET(EnergyState, Points));
    BML_AS_PROPERTY("EnergyState", "int Lives", asOFFSET(EnergyState, Lives));
    BML_AS_PROPERTY("EnergyState", "int StartPoints", asOFFSET(EnergyState, StartPoints));
    BML_AS_PROPERTY("EnergyState", "int StartLives", asOFFSET(EnergyState, StartLives));
    BML_AS_PROPERTY("EnergyState", "float TimeFactor", asOFFSET(EnergyState, TimeFactor));
    BML_AS_PROPERTY("EnergyState", "int LifeBonus", asOFFSET(EnergyState, LifeBonus));
    BML_AS_PROPERTY("CatalogEntry", "int Bonus", asOFFSET(CatalogEntry, Bonus));
    BML_AS_PROPERTY("CatalogEntry", "int Music", asOFFSET(CatalogEntry, Music));
    BML_AS_PROPERTY("CatalogEntry", "bool FileTruncated", asOFFSET(CatalogEntry, FileTruncated));
    BML_AS_PROPERTY("CatalogEntry", "bool StartBallTruncated", asOFFSET(CatalogEntry, StartBallTruncated));
    BML_AS_PROPERTY("CatalogEntry", "bool SkyTruncated", asOFFSET(CatalogEntry, SkyTruncated));
    BML_AS_PROPERTY("Checkpoint", "BML::Mat4 Matrix", asOFFSET(Checkpoint, Matrix));
#undef BML_AS_PROPERTY
    return Register(engine, engine->RegisterObjectMethod("LevelState", "CKObject@ BorrowActiveBall() const", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&BorrowActiveBall), asCALL_GENERIC), "LevelState::BorrowActiveBall", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogEntry", "string get_File() const", BML_AS_STRING_FIELD_GETTER(CatalogEntry, File), asCALL_GENERIC), "CatalogEntry::File", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogEntry", "string get_StartBall() const", BML_AS_STRING_FIELD_GETTER(CatalogEntry, StartBall), asCALL_GENERIC), "CatalogEntry::StartBall", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("CatalogEntry", "string get_Sky() const", BML_AS_STRING_FIELD_GETTER(CatalogEntry, Sky), asCALL_GENERIC), "CatalogEntry::Sky", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Checkpoint", "CKObject@ BorrowObject() const", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&BorrowCheckpointObject), asCALL_GENERIC), "Checkpoint::BorrowObject", errorMessage) &&
           Register(engine, engine->RegisterObjectMethod("Resetpoint", "CKObject@ BorrowObject() const", BML_AS_GENERIC_OBJECT_FIRST_FUNCTION(&BorrowResetpointObject), asCALL_GENERIC), "Resetpoint::BorrowObject", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadLevel(LevelState &out state)", BML_AS_GENERIC_FUNCTION(&ReadLevel), asCALL_GENERIC), "Gameplay::ReadLevel", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadEnergy(EnergyState &out state)", BML_AS_GENERIC_FUNCTION(&ReadEnergy), asCALL_GENERIC), "Gameplay::ReadEnergy", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadCatalogCount(int &out count)", BML_AS_GENERIC_FUNCTION(&ReadCatalogCount), asCALL_GENERIC), "Gameplay::ReadCatalogCount", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadCatalogEntry(int index, CatalogEntry &out entry)", BML_AS_GENERIC_FUNCTION(&ReadCatalogEntry), asCALL_GENERIC), "Gameplay::ReadCatalogEntry", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadCheckpointCount(int &out count)", BML_AS_GENERIC_FUNCTION(&ReadCheckpointCount), asCALL_GENERIC), "Gameplay::ReadCheckpointCount", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadCheckpoint(int index, Checkpoint &out checkpoint)", BML_AS_GENERIC_FUNCTION(&ReadCheckpoint), asCALL_GENERIC), "Gameplay::ReadCheckpoint", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadResetpointCount(int &out count)", BML_AS_GENERIC_FUNCTION(&ReadResetpointCount), asCALL_GENERIC), "Gameplay::ReadResetpointCount", errorMessage) &&
           Register(engine, engine->RegisterGlobalFunction("int ReadResetpoint(int index, Resetpoint &out resetpoint)", BML_AS_GENERIC_FUNCTION(&ReadResetpoint), asCALL_GENERIC), "Gameplay::ReadResetpoint", errorMessage) &&
           Register(engine, engine->SetDefaultNamespace(""), "namespace reset", errorMessage);
}

} // namespace

int RegisterScriptBuiltinFacade(asIScriptEngine *engine, const char **errorMessage) {
    if (!engine) {
        g_FacadeRegistrationError = "The built-in script facade received a null engine.";
        if (errorMessage)
            *errorMessage = g_FacadeRegistrationError.c_str();
        return asERROR;
    }
    if (!RegisterRuntime(engine, errorMessage) ||
        !RegisterGameplay(engine, errorMessage)) {
        engine->SetDefaultNamespace("");
        return asERROR;
    }
    return asSUCCESS;
}
