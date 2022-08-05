#include <string>
#include <vector>
#include <functional>

#include "Defines.h"
#include "ModLoader.h"
#include "ModManager.h"
#include "ScriptHelper.h"
#include "Util.h"

void AddDataPath(const char *path);
bool ResolveFileName(std::string &file);

namespace {
    std::vector<std::string> g_DataPaths;
    std::vector<std::string> g_TexturePaths;
    std::vector<std::string> g_SoundPaths;

    CKBEHAVIORFCT g_ObjectLoad = nullptr;
};

int ObjectLoad(const CKBehaviorContext &behcontext) {
    CKBehavior *beh = behcontext.Behavior;
    bool active = beh->IsInputActive(0);

    std::string file = (const char *) beh->GetInputParameterReadDataPtr(0);
    if (ResolveFileName(file))
        ScriptHelper::SetParamString(beh->GetInputParameter(0)->GetDirectSource(), file.c_str());

    int result = g_ObjectLoad(behcontext);

    if (active) {
        const char *filename = (const char *) beh->GetInputParameterReadDataPtr(0);
        const char *mastername = (const char *) beh->GetInputParameterReadDataPtr(1);
        CK_CLASSID cid = CKCID_3DOBJECT;
        beh->GetInputParameterValue(2, &cid);
        CKBOOL addToScene = TRUE, reuseMeshes = FALSE, reuseMaterials = FALSE;
        beh->GetInputParameterValue(3, &addToScene);
        beh->GetInputParameterValue(4, &reuseMeshes);
        beh->GetInputParameterValue(5, &reuseMaterials);

        CKBOOL dynamic = TRUE;
        beh->GetLocalParameterValue(0, &dynamic);

        XObjectArray *oarray = *(XObjectArray **) beh->GetOutputParameterWriteDataPtr(0);
        CKObject *masterobject = beh->GetOutputParameterObject(1);
        CKBOOL isMap = !strcmp(beh->GetOwnerScript()->GetName(), "Levelinit_build");

        ModLoader::GetInstance().BroadcastCallback(&IMod::OnLoadObject,
                                                                              filename, isMap, mastername, cid,
                                                                              addToScene, reuseMeshes,
                                                                              reuseMaterials, dynamic, oarray,
                                                                              masterobject);

        for (CK_ID *id = oarray->Begin(); id != oarray->End(); id++) {
            CKObject *obj = ModLoader::GetInstance().GetCKContext()->GetObject(*id);
            if (obj->GetClassID() == CKCID_BEHAVIOR) {
                auto *behavior = (CKBehavior *) obj;
                if (behavior->GetType() == CKBEHAVIORTYPE_SCRIPT) {
                    ModLoader::GetInstance().BroadcastCallback(&IMod::OnLoadScript, filename, behavior);
                }
            }
        }
    }

    return result;
}

void AddDataPath(const char *path) {
    static std::vector<std::string> paths;
    if (std::find(paths.cbegin(), paths.cend(), path) != paths.cend())
        return;

    char fullPath[512];
    VxGetCurrentDirectory(fullPath);

    char *p = &fullPath[strlen(fullPath)];
    sprintf(p, "\\%s", path);
    g_DataPaths.emplace_back(fullPath);

    p = &fullPath[strlen(fullPath)];
    sprintf(p, "\\Textures");
    g_TexturePaths.emplace_back(fullPath);
    sprintf(p, "\\Sounds");
    g_SoundPaths.emplace_back(fullPath);

    paths.emplace_back(path);
}

bool ResolveFileName(std::string &file) {
    bool ret = false;

    if (file.size() > 3 && file[1] == ':')
        return true;

    int catIdx = -1;
    if (IsVirtoolsFile(file.c_str()))
        catIdx = DATA_PATH_IDX;
    else if (IsTextureFile(file.c_str()))
        catIdx = BITMAP_PATH_IDX;
    else if (IsSoundFile(file.c_str()))
        catIdx = SOUND_PATH_IDX;

    switch (catIdx) {
        case DATA_PATH_IDX:
            for (const auto &path: g_DataPaths) {
                auto fullPath = path + file;
                if (IsFileExist(fullPath.c_str())) {
                    file = fullPath;
                    ret = true;
                }
            }
            break;
        case BITMAP_PATH_IDX:
            for (const auto &path: g_TexturePaths) {
                auto fullPath = path + file;
                if (IsFileExist(fullPath.c_str())) {
                    file = fullPath;
                    ret = true;
                }
            }
            break;
        case SOUND_PATH_IDX:
            for (const auto &path: g_SoundPaths) {
                auto fullPath = path + file;
                if (IsFileExist(fullPath.c_str())) {
                    file = fullPath;
                    ret = true;
                }
            }
            break;
        default:
            break;
    }

    return ret;
}

bool HookObjectLoad() {
    CKBehaviorPrototype *objectLoadProto = CKGetPrototypeFromGuid(VT_OBJECTLOAD);
    if (!objectLoadProto) return false;
    if (!g_ObjectLoad) g_ObjectLoad = objectLoadProto->GetFunction();
    objectLoadProto->SetFunction(&ObjectLoad);
    return true;
}
