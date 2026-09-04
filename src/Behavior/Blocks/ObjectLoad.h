#ifndef BML_BEHAVIOR_OBJECTLOAD_H
#define BML_BEHAVIOR_OBJECTLOAD_H

#include <string>

#include "Behavior/Runtime.h"

namespace BML::Behavior::ObjectLoad {

struct Options {
    std::string File;
    std::string MasterName;
    CK_CLASSID FilterClass = CKCID_3DOBJECT;
    CKBOOL AddToScene = TRUE;
    CKBOOL ReuseMeshes = TRUE;
    CKBOOL ReuseMaterials = TRUE;
    CKBOOL Dynamic = TRUE;
};

Spec Make(const Options &options);

} // namespace BML::Behavior::ObjectLoad

#endif // BML_BEHAVIOR_OBJECTLOAD_H
