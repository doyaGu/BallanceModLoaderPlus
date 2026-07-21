#ifndef BML_RUNTIME_H
#define BML_RUNTIME_H

#include "BML/Generated/bml_runtime_api.h"
#include "BML/InteropClient.h"

namespace BML::Runtime {

struct State {
    bool InGame = false;
    bool InLevel = false;
    bool Paused = false;
    bool Playing = false;
    bool CheatEnabled = false;
};

struct Clock {
    float TimeMs = 0.0f;
    float AbsoluteMs = 0.0f;
    float DeltaMs = 0.0f;
    int Frame = 0;
};

struct Score {
    float SR = 0.0f;
    int HS = 0;
};

inline int RequireApi() {
    return Interop::RequireApi(Interop::Generated::Bml::Runtime::Descriptor);
}

inline int ReadState(State &out) {
    Interop::Record record;
    int status = RequireApi();
    if (status == BML_OK) status = Interop::ReadResource(Interop::Generated::Bml::Runtime::ApiId, "state", record);
    State value{};
    if (status == BML_OK) status = record.GetBool(Interop::Generated::Bml::Runtime::RuntimeStateField::InGame, value.InGame);
    if (status == BML_OK) status = record.GetBool(Interop::Generated::Bml::Runtime::RuntimeStateField::InLevel, value.InLevel);
    if (status == BML_OK) status = record.GetBool(Interop::Generated::Bml::Runtime::RuntimeStateField::Paused, value.Paused);
    if (status == BML_OK) status = record.GetBool(Interop::Generated::Bml::Runtime::RuntimeStateField::Playing, value.Playing);
    if (status == BML_OK) status = record.GetBool(Interop::Generated::Bml::Runtime::RuntimeStateField::CheatEnabled, value.CheatEnabled);
    if (status == BML_OK) out = value;
    return status;
}

inline int ReadClock(Clock &out) {
    Interop::Record record;
    int status = RequireApi();
    if (status == BML_OK) status = Interop::ReadResource(Interop::Generated::Bml::Runtime::ApiId, "clock", record);
    Clock value{};
    if (status == BML_OK) status = record.GetFloat(Interop::Generated::Bml::Runtime::ClockStateField::TimeMs, value.TimeMs);
    if (status == BML_OK) status = record.GetFloat(Interop::Generated::Bml::Runtime::ClockStateField::AbsoluteMs, value.AbsoluteMs);
    if (status == BML_OK) status = record.GetFloat(Interop::Generated::Bml::Runtime::ClockStateField::DeltaMs, value.DeltaMs);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Runtime::ClockStateField::Frame, value.Frame);
    if (status == BML_OK) out = value;
    return status;
}

inline int ReadScore(Score &out) {
    Interop::Record record;
    int status = RequireApi();
    if (status == BML_OK) status = Interop::ReadResource(Interop::Generated::Bml::Runtime::ApiId, "score", record);
    Score value{};
    if (status == BML_OK) status = record.GetFloat(Interop::Generated::Bml::Runtime::ScoreStateField::Sr, value.SR);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Runtime::ScoreStateField::Hs, value.HS);
    if (status == BML_OK) out = value;
    return status;
}

} // namespace BML::Runtime

#endif // BML_RUNTIME_H
