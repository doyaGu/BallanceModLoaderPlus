#ifndef BML_IMOD_H
#define BML_IMOD_H

#include <string>
#include <vector>

#include "CKAll.h"

#include "BML/BML.h"
#include "BML/IBML.h"
#include "BML/ILogger.h"
#include "BML/IConfig.h"

struct BMLVersion {
    int major, minor, patch;

    BMLVersion() : major(BML_MAJOR_VERSION), minor(BML_MINOR_VERSION), patch(BML_PATCH_VERSION) {}
    BMLVersion(int mj, int mn, int bd) : major(mj), minor(mn), patch(bd) {}

    bool operator<(const BMLVersion &o) const {
        if (major == o.major) {
            if (minor == o.minor)
                return patch < o.patch;
            return minor < o.minor;
        }
        return major < o.major;
    }

    bool operator>=(const BMLVersion &o) const {
        return !(*this < o);
    }

    bool operator==(const BMLVersion &o) const {
        return major == o.major && minor == o.minor && patch == o.patch;
    }

    std::string ToString() const {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d.%d.%d", major, minor, patch);
        return buf;
    }
};

#define DECLARE_BML_VERSION \
    BMLVersion GetBMLVersion() override { return { BML_MAJOR_VERSION, BML_MINOR_VERSION, BML_PATCH_VERSION }; }

struct ModDependency {
    char *id;
    BMLVersion minVersion;
    int optional; // (0 = required, 1 = optional)

    ModDependency() : id(nullptr), optional(0) {}
    ModDependency(const char *modId, const BMLVersion &version, int isOptional = 0)
        : minVersion(version), optional(isOptional) {
        if (modId) {
            size_t len = strlen(modId);
            id = static_cast<char *>(BML_Malloc(len + 1));
            strcpy(id, modId);
        } else {
            id = nullptr;
        }
    }

    ModDependency(const ModDependency &other) : minVersion(other.minVersion), optional(other.optional) {
        if (other.id) {
            size_t len = strlen(other.id);
            id = static_cast<char *>(BML_Malloc(len + 1));
            strcpy(id, other.id);
        } else {
            id = nullptr;
        }
    }

    ModDependency &operator=(const ModDependency &other) {
        if (this != &other) {
            delete[] id;

            if (other.id) {
                size_t len = strlen(other.id);
                id = static_cast<char *>(BML_Malloc(len + 1));
                strcpy(id, other.id);
            } else {
                id = nullptr;
            }

            minVersion = other.minVersion;
            optional = other.optional;
        }
        return *this;
    }

    ~ModDependency() {
        if (id)
            BML_Free(id);
    }

    int operator==(const ModDependency &other) const {
        if (id == nullptr || other.id == nullptr)
            return id == other.id;
        return strcmp(id, other.id) == 0;
    }

    int operator!=(const ModDependency &other) const {
        return !(*this == other);
    }
};

class BML_EXPORT IMod : public IMessageReceiver {
public:
    explicit IMod(IBML *bml) : m_BML(bml) {}
    virtual ~IMod();

    virtual const char *GetID() = 0;
    virtual const char *GetVersion() = 0;
    virtual const char *GetName() = 0;
    virtual const char *GetAuthor() = 0;
    virtual const char *GetDescription() = 0;
    virtual BMLVersion GetBMLVersion() = 0;

    virtual void OnLoad() {}
    virtual void OnUnload() {}
    virtual void OnModifyConfig(const char *category, const char *key, IProperty *prop) {}
    virtual void OnLoadObject(const char *filename, CKBOOL isMap, const char *masterName, CK_CLASSID filterClass,
                              CKBOOL addToScene, CKBOOL reuseMeshes, CKBOOL reuseMaterials, CKBOOL dynamic,
                              XObjectArray *objArray, CKObject *masterObj) {}
    virtual void OnLoadScript(const char *filename, CKBehavior *script) {}

    virtual void OnProcess() {}
    virtual void OnRender(CK_RENDER_FLAGS flags) {}

    virtual void OnCheatEnabled(bool enable) {}

    virtual void OnPhysicalize(CK3dEntity *target, CKBOOL fixed, float friction, float elasticity, float mass,
                               const char *collGroup, CKBOOL startFrozen, CKBOOL enableColl, CKBOOL calcMassCenter,
                               float linearDamp, float rotDamp, const char *collSurface, VxVector massCenter,
                               int convexCnt, CKMesh **convexMesh, int ballCnt, VxVector *ballCenter, float *ballRadius,
                               int concaveCnt, CKMesh **concaveMesh) {}
    virtual void OnUnphysicalize(CK3dEntity *target) {}

    virtual void OnPreCommandExecute(ICommand *command, const std::vector<std::string> &args) {}
    virtual void OnPostCommandExecute(ICommand *command, const std::vector<std::string> &args) {}

protected:
    virtual ILogger *GetLogger() final;
    virtual IConfig *GetConfig() final;

    bool AddDependency(const char *modId, const BMLVersion &minVersion = BMLVersion(0, 0, 0)) {
        return m_BML->RegisterDependency(this, modId, minVersion.major, minVersion.minor, minVersion.patch) == BML_OK;
    }

    bool AddOptionalDependency(const char *modId, const BMLVersion &minVersion = BMLVersion(0, 0, 0)) {
        return m_BML->RegisterOptionalDependency(this, modId, minVersion.major, minVersion.minor, minVersion.patch) == BML_OK;
    }

    bool CheckDependencies() {
        return m_BML->CheckDependencies(this) != 0;
    }

    int GetDependencyCount() {
        return m_BML->GetDependencyCount(this);
    }

    bool ClearDependencies() {
        return m_BML->ClearDependencies(this) == BML_OK;
    }

    IBML *m_BML = nullptr;

private:
    ILogger *m_Logger = nullptr;
    IConfig *m_Config = nullptr;
};

#endif // BML_IMOD_H
