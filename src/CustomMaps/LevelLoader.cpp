#include "CustomMaps/LevelLoader.h"

#include <cstdint>
#include <utility>
#include <variant>

#include "CKAll.h"

#include "BML/Guids/Logics.h"
#include "BML/Guids/Narratives.h"

namespace Behavior = BML::Behavior;

namespace CustomMap {

LevelLoader::Transaction::Transaction(
    Behavior::Plan::Instance instance, Behavior::Edit::Port mapFile,
    Behavior::Edit::Port levelRow, Behavior::Edit::Port loadCustom,
    Snapshot previous)
    : m_Instance(std::move(instance)),
      m_MapFile(std::move(mapFile)),
      m_LevelRow(std::move(levelRow)),
      m_LoadCustom(std::move(loadCustom)),
      m_Previous(std::move(previous)) {}

LevelLoader::Transaction::operator bool() const noexcept {
    return static_cast<bool>(m_Instance);
}

Behavior::Result<void> LevelLoader::Transaction::Stage(
    std::string_view mapFile, int level, CKDataArray *currentLevel) const {
    if (!*this) {
        return Failure(BML_ERROR_INVALID_HANDLE, Behavior::Error::StateInvalid,
                       "The custom map level-loader transaction is not active.");
    }

    auto mapFileSet = m_Instance.Set(m_MapFile, mapFile);
    if (!mapFileSet)
        return AddContext(std::move(mapFileSet), "Could not set Object Load.File");

    int currentLevelValue = level;
    if (!currentLevel ||
        currentLevel->SetElementValue(0, 0, &currentLevelValue) == 0) {
        return Failure(BML_ERROR_FAIL, Behavior::Error::Unavailable,
                       "Could not update CurrentLevel.");
    }

    auto rowSet = m_Instance.Set(m_LevelRow, level - 1);
    if (!rowSet)
        return AddContext(std::move(rowSet), "Could not set AllLevel row");

    auto customSet = m_Instance.Set(m_LoadCustom, true);
    if (!customSet) {
        return AddContext(std::move(customSet),
                          "Could not enable the custom-level route");
    }
    return Behavior::Result<void>::Success();
}

Behavior::Result<void> LevelLoader::Transaction::Rollback(
    CKDataArray *currentLevel) const {
    if (!*this) {
        return Failure(BML_ERROR_INVALID_HANDLE, Behavior::Error::StateInvalid,
                       "The custom map level-loader transaction is not active.");
    }

    Behavior::Result<void> restored = Behavior::Result<void>::Success();
    RememberFirstFailure(
        restored, AddContext(m_Instance.Set(m_LoadCustom, m_Previous.LoadCustom),
                             "Could not restore the custom-level route"));
    RememberFirstFailure(
        restored, AddContext(m_Instance.Set(m_LevelRow, m_Previous.LevelRow),
                             "Could not restore AllLevel row"));

    int currentLevelValue = m_Previous.CurrentLevel;
    if (!currentLevel ||
        currentLevel->SetElementValue(0, 0, &currentLevelValue) == 0) {
        RememberFirstFailure(
            restored,
            Failure(BML_ERROR_FAIL, Behavior::Error::Unavailable,
                    "Could not restore CurrentLevel."));
    }
    RememberFirstFailure(
        restored, AddContext(m_Instance.Set(m_MapFile, m_Previous.MapFile),
                             "Could not restore Object Load.File"));
    return restored;
}

Behavior::Result<void> LevelLoader::Transaction::ClearRoute() const {
    if (!*this) {
        return Failure(BML_ERROR_INVALID_HANDLE, Behavior::Error::StateInvalid,
                       "The custom map level-loader transaction is not active.");
    }
    return AddContext(m_Instance.Set(m_LoadCustom, false),
                      "Could not clear the custom-level route");
}

Behavior::Result<void> LevelLoader::Install(Behavior::Session &behavior) {
    if (!behavior) {
        return Failure(BML_ERROR_INVALID_HANDLE,
                       Behavior::Error::OwnerUnavailable,
                       "Behavior authoring is unavailable.");
    }
    if (m_Plan || m_Retiring) {
        return Failure(BML_ERROR_BUSY, Behavior::Error::Busy,
                       "The custom map level-loader Plan is already active.");
    }

    Behavior::Edit edit;
    auto level = edit.Root().Require("Load LevelXX").Graph();
    Behavior::NodePattern firstPattern("Get Cell");
    firstPattern.Prototype(VT_LOGICS_GETCELL);
    Behavior::NodePattern loaderPattern("Object Load");
    loaderPattern.Prototype(VT_NARRATIVES_OBJECTLOAD);

    const auto entry = level.Leaving(level.Root().In(0));
    const auto originalFirst = level.Next(level.Root().In(0),
                                          std::move(firstPattern));
    const auto loader = level.Require(std::move(loaderPattern));
    const auto selector = level.Add(behavior.Use(VT_LOGICS_BINARYSWITCH));
    const auto loadCustom = level.AppendLocal("Custom Level", CKPGUID_BOOL);
    const auto mapFile = loader.Pin("File", CKPGUID_STRING);
    const auto levelRow = level.Root().Local("AllLevel row", CKPGUID_INT);

    level.Bind(selector.Pin(0, CKPGUID_BOOL), loadCustom);
    level.Flow(level.Root().In(0), selector.In(0));
    level.Reconnect(entry, selector.Out(1), originalFirst.In(0));
    level.Flow(selector.Out(0), loader.In(0));

    auto submitted = behavior.Plan(
        "Custom map level loader", Behavior::Scripts::One("Levelinit_build"),
        edit);
    if (!submitted) {
        return Behavior::Result<void>::Failure(submitted.Code(),
                                               submitted.GetStatus());
    }

    m_MapFile = mapFile;
    m_LevelRow = levelRow;
    m_LoadCustom = loadCustom;
    m_Plan = submitted.Take();
    return Behavior::Result<void>::Success(submitted.GetStatus());
}

Behavior::Result<void> LevelLoader::Refresh() {
    if (m_Retiring && m_ClosePending) {
        auto closed = Close();
        if (!closed) {
            return Behavior::Result<void>::Failure(closed.Code(),
                                                   closed.GetStatus());
        }
        return Behavior::Result<void>::Success();
    }
    if (m_Retiring)
        return Behavior::Result<void>::Success();
    if (m_Instance)
        return Behavior::Result<void>::Success();
    if (!m_Plan)
        return Behavior::Result<void>::Success();

    auto instances = m_Plan.Instances();
    if (!instances) {
        return Behavior::Result<void>::Failure(
            instances.Code(), instances.GetStatus());
    }
    if (instances->empty()) {
        auto info = m_Plan.Info();
        if (!info) {
            return Behavior::Result<void>::Failure(
                info.Code(), info.GetStatus());
        }
        const Behavior::Status &failure =
            info->ApplyFailure.Error != Behavior::Error::None
                ? info->ApplyFailure
                : info->LastStatus;
        if (failure.Error != Behavior::Error::None) {
            return Behavior::Result<void>::Failure(BML_ERROR_FAIL, failure);
        }
        return Behavior::Result<void>::Success();
    }
    if (instances->size() != 1) {
        return Failure(
            BML_ERROR_INVALID_PARAMETER, Behavior::Error::TargetCardinality,
            "The custom map level-loader Plan matched more than one Script.");
    }

    m_Instance = std::move(instances->front());
    return Behavior::Result<void>::Success();
}

Behavior::Result<LevelLoader::Transaction> LevelLoader::Begin(
    CKDataArray *currentLevel) const {
    if (!m_Instance) {
        return Failure<Transaction>(
            BML_ERROR_INVALID_HANDLE, Behavior::Error::StateInvalid,
            "The custom map level-loader instance is not ready.");
    }

    auto mapFile = Read<std::string>(m_Instance.value(), m_MapFile,
                                     "Object Load.File");
    if (!mapFile) {
        return Behavior::Result<Transaction>::Failure(mapFile.Code(),
                                                      mapFile.GetStatus());
    }

    Transaction::Snapshot snapshot;
    snapshot.MapFile = mapFile.Take();
    if (!currentLevel ||
        currentLevel->GetElementValue(0, 0, &snapshot.CurrentLevel) == 0) {
        return Failure<Transaction>(BML_ERROR_FAIL,
                                    Behavior::Error::Unavailable,
                                    "Could not read CurrentLevel.");
    }

    auto levelRow = Read<std::int32_t>(m_Instance.value(), m_LevelRow,
                                       "AllLevel row");
    if (!levelRow) {
        return Behavior::Result<Transaction>::Failure(levelRow.Code(),
                                                      levelRow.GetStatus());
    }
    snapshot.LevelRow = levelRow.Take();

    auto loadCustom = Read<bool>(m_Instance.value(), m_LoadCustom,
                                 "Custom Level");
    if (!loadCustom) {
        return Behavior::Result<Transaction>::Failure(loadCustom.Code(),
                                                      loadCustom.GetStatus());
    }
    snapshot.LoadCustom = loadCustom.Take();

    return Behavior::Result<Transaction>::Success(Transaction(
        m_Instance.value(), m_MapFile, m_LevelRow, m_LoadCustom,
        std::move(snapshot)));
}

Behavior::Result<Behavior::CloseState> LevelLoader::Close() {
    m_Instance.reset();
    m_Retiring = true;
    m_ClosePending = false;
    if (!m_Plan) {
        ClearPorts();
        m_Retiring = false;
        return Behavior::Result<Behavior::CloseState>::Success(
            Behavior::CloseState::Closed);
    }

    auto closed = m_Plan.Close();
    if (!closed)
        return closed;
    if (closed.Value() == Behavior::CloseState::Closing) {
        m_ClosePending = true;
    } else {
        m_Plan = {};
        ClearPorts();
        m_Retiring = false;
    }
    return closed;
}

void LevelLoader::Invalidate() noexcept {
    m_Instance.reset();
}

bool LevelLoader::IsReady() const noexcept {
    return m_Instance.has_value();
}

bool LevelLoader::IsRetiring() const noexcept {
    return m_Retiring;
}

template <class T>
Behavior::Result<T> LevelLoader::Read(
    const Behavior::Plan::Instance &instance,
    const Behavior::Edit::Port &port, std::string_view name) {
    auto observed = instance.Read(port);
    if (!observed) {
        return Behavior::Result<T>::Failure(
            observed.Code(),
            AddContext(observed.GetStatus(),
                       "Could not read " + std::string(name)));
    }
    if (observed->State != Behavior::ObservationState::Available) {
        return Failure<T>(BML_ERROR_NOT_FOUND, Behavior::Error::Unavailable,
                          std::string(name) + " is not currently available.");
    }
    const T *value = std::get_if<T>(&observed->Data);
    if (!value) {
        return Failure<T>(
            BML_ERROR_INVALID_PARAMETER, Behavior::Error::TypeMismatch,
            std::string(name) + " has an unexpected Behavior value type.");
    }
    return Behavior::Result<T>::Success(*value);
}

Behavior::Status LevelLoader::AddContext(Behavior::Status status,
                                         std::string message) {
    if (!status.Message.empty())
        message += ": " + status.Message;
    status.Message = std::move(message);
    return status;
}

Behavior::Result<void> LevelLoader::AddContext(
    Behavior::Result<void> result, std::string message) {
    if (result)
        return result;
    return Behavior::Result<void>::Failure(
        result.Code(), AddContext(result.GetStatus(), std::move(message)));
}

template <class T>
Behavior::Result<T> LevelLoader::Failure(
    int code, Behavior::Error error, std::string message) {
    Behavior::Status status;
    status.Error = error;
    status.Phase = Behavior::Phase::Binding;
    status.Message = std::move(message);
    return Behavior::Result<T>::Failure(code, std::move(status));
}

void LevelLoader::RememberFirstFailure(
    Behavior::Result<void> &result, Behavior::Result<void> candidate) {
    if (result && !candidate)
        result = std::move(candidate);
}

void LevelLoader::ClearPorts() {
    m_MapFile = {};
    m_LevelRow = {};
    m_LoadCustom = {};
}

} // namespace CustomMap
