#ifndef BML_IMOD_H
#define BML_IMOD_H

#include <string>
#include <vector>

#include "CKAll.h"

#include "BML/Defines.h"
#include "BML/IMessageReceiver.h"
#include "BML/ILogger.h"
#include "BML/IConfig.h"
#include "BML/ICommand.h"

class IBML;

struct BMLVersion {
    int major, minor, build;

    BMLVersion() : major(BML_MAJOR_VER), minor(BML_MINOR_VER), build(BML_PATCH_VER) {}
    BMLVersion(int mj, int mn, int bd) : major(mj), minor(mn), build(bd) {}

    bool operator<(const BMLVersion &o) const {
        if (major == o.major) {
            if (minor == o.minor)
                return build < o.build;
            return minor < o.minor;
        }
        return major < o.major;
    }

    bool operator>=(const BMLVersion &o) const {
        return !(*this < o);
    }
};

#define DECLARE_BML_VERSION \
    BMLVersion GetBMLVersion() override { return { BML_MAJOR_VER, BML_MINOR_VER, BML_PATCH_VER }; }

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

    IBML *m_BML = nullptr;

private:
    ILogger *m_Logger = nullptr;
    IConfig *m_Config = nullptr;
};

#endif // BML_IMOD_H