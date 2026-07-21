#ifndef BML_GAMEPLAY_H
#define BML_GAMEPLAY_H

#include "BML/Generated/bml_gameplay_api.h"
#include "BML/InteropClient.h"

#include <new>
#include <string>
#include <utility>
#include <vector>

namespace BML::Gameplay {

using ObjectRef = Interop::ObjectRef;
using Mat4 = Interop::Mat4;

struct LevelState {
    int Id = 0;
    ObjectRef ActiveBall{};
    Mat4 ResetMatrix{};
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
};

struct Checkpoint {
    Mat4 Matrix{};
    ObjectRef Object{};
};

struct Resetpoint {
    ObjectRef Object{};
};

inline int RequireApi() {
    return Interop::RequireApi(Interop::Generated::Bml::Gameplay::Descriptor);
}

inline int ReadLevel(LevelState &out) {
    Interop::Record record;
    int status = RequireApi();
    if (status == BML_OK) status = Interop::ReadResource(Interop::Generated::Bml::Gameplay::ApiId, "level", record);
    LevelState value{};
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Gameplay::LevelStateField::Id, value.Id);
    if (status == BML_OK) status = record.GetObjectRef(Interop::Generated::Bml::Gameplay::LevelStateField::ActiveBall, value.ActiveBall);
    if (status == BML_OK) status = record.GetMat4(Interop::Generated::Bml::Gameplay::LevelStateField::ResetMatrix, value.ResetMatrix);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Gameplay::LevelStateField::Points, value.Points);
    if (status == BML_OK) out = value;
    return status;
}

inline int ReadEnergy(EnergyState &out) {
    Interop::Record record;
    int status = RequireApi();
    if (status == BML_OK) status = Interop::ReadResource(Interop::Generated::Bml::Gameplay::ApiId, "energy", record);
    EnergyState value{};
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Gameplay::EnergyStateField::Points, value.Points);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Gameplay::EnergyStateField::Lives, value.Lives);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Gameplay::EnergyStateField::StartPoints, value.StartPoints);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Gameplay::EnergyStateField::StartLives, value.StartLives);
    if (status == BML_OK) status = record.GetFloat(Interop::Generated::Bml::Gameplay::EnergyStateField::TimeFactor, value.TimeFactor);
    if (status == BML_OK) status = record.GetInt(Interop::Generated::Bml::Gameplay::EnergyStateField::LifeBonus, value.LifeBonus);
    if (status == BML_OK) out = value;
    return status;
}

class Catalog final {
public:
    int Open() {
        const int status = RequireApi();
        return status == BML_OK ? Interop::OpenCollection(Interop::Generated::Bml::Gameplay::ApiId, "catalog", m_Cursor) : status;
    }
    bool IsOpen() const { return m_Cursor.Valid(); }
    int Close() { return m_Cursor.Close(); }
    int Next(uint32_t limit, std::vector<CatalogEntry> &out, bool &complete) {
        std::vector<Interop::Record> records;
        const int status = m_Cursor.Next(limit, records, complete);
        if (status != BML_OK) return status;
        std::vector<CatalogEntry> values;
        try {
            values.reserve(records.size());
            for (Interop::Record &record : records) {
                CatalogEntry value{};
                int read = record.GetString(Interop::Generated::Bml::Gameplay::CatalogEntryField::File, value.File);
                if (read == BML_OK) read = record.GetString(Interop::Generated::Bml::Gameplay::CatalogEntryField::StartBall, value.StartBall);
                if (read == BML_OK) read = record.GetString(Interop::Generated::Bml::Gameplay::CatalogEntryField::Sky, value.Sky);
                if (read == BML_OK) read = record.GetInt(Interop::Generated::Bml::Gameplay::CatalogEntryField::Bonus, value.Bonus);
                if (read == BML_OK) read = record.GetInt(Interop::Generated::Bml::Gameplay::CatalogEntryField::Music, value.Music);
                if (read != BML_OK) return read;
                values.push_back(std::move(value));
            }
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
        out = std::move(values);
        return BML_OK;
    }
private:
    Interop::Collection m_Cursor;
};

class Checkpoints final {
public:
    int Open() {
        const int status = RequireApi();
        return status == BML_OK ? Interop::OpenCollection(Interop::Generated::Bml::Gameplay::ApiId, "checkpoints", m_Cursor) : status;
    }
    bool IsOpen() const { return m_Cursor.Valid(); }
    int Close() { return m_Cursor.Close(); }
    int Next(uint32_t limit, std::vector<Checkpoint> &out, bool &complete) {
        std::vector<Interop::Record> records;
        const int status = m_Cursor.Next(limit, records, complete);
        if (status != BML_OK) return status;
        std::vector<Checkpoint> values;
        try {
            values.reserve(records.size());
            for (Interop::Record &record : records) {
                Checkpoint value{};
                int read = record.GetMat4(Interop::Generated::Bml::Gameplay::CheckpointField::Matrix, value.Matrix);
                if (read == BML_OK) read = record.GetObjectRef(Interop::Generated::Bml::Gameplay::CheckpointField::Object, value.Object);
                if (read != BML_OK) return read;
                values.push_back(value);
            }
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
        out = std::move(values);
        return BML_OK;
    }
private:
    Interop::Collection m_Cursor;
};

class Resetpoints final {
public:
    int Open() {
        const int status = RequireApi();
        return status == BML_OK ? Interop::OpenCollection(Interop::Generated::Bml::Gameplay::ApiId, "resetpoints", m_Cursor) : status;
    }
    bool IsOpen() const { return m_Cursor.Valid(); }
    int Close() { return m_Cursor.Close(); }
    int Next(uint32_t limit, std::vector<Resetpoint> &out, bool &complete) {
        std::vector<Interop::Record> records;
        const int status = m_Cursor.Next(limit, records, complete);
        if (status != BML_OK) return status;
        std::vector<Resetpoint> values;
        try {
            values.reserve(records.size());
            for (Interop::Record &record : records) {
                Resetpoint value{};
                const int read = record.GetObjectRef(Interop::Generated::Bml::Gameplay::ResetpointField::Object, value.Object);
                if (read != BML_OK) return read;
                values.push_back(value);
            }
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        }
        out = std::move(values);
        return BML_OK;
    }
private:
    Interop::Collection m_Cursor;
};

} // namespace BML::Gameplay

#endif // BML_GAMEPLAY_H
