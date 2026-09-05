// The Object Load Building Block: a file, a master name, and what to reuse.
#ifndef BML_BEHAVIOR_BLOCKS_OBJECTLOAD_HPP
#define BML_BEHAVIOR_BLOCKS_OBJECTLOAD_HPP

#include <string>
#include <string_view>

#include "CKAll.h"
#ifdef BML_BEHAVIOR_INTERNAL
#include "Behavior/Blocks/Definition.h"
#else
#include "BML/Behavior/Detail/Blocks.hpp"
#endif
#include "BML/Guids/Narratives.h"

namespace BML::Behavior::Blocks {
namespace ObjectLoad {

struct Options {
    std::string File;
    std::string MasterName;
    CK_CLASSID FilterClass = CKCID_3DOBJECT;
    CKBOOL AddToScene = TRUE;
    CKBOOL ReuseMeshes = TRUE;
    CKBOOL ReuseMaterials = TRUE;
    CKBOOL Dynamic = TRUE;
};

namespace Detail {
template <class Definition>
void Define(Definition &block, const Options &options) {
    block.Pin(0, CKPGUID_STRING, std::string_view(options.File));
    block.Pin(1, CKPGUID_STRING, std::string_view(options.MasterName));
    block.Pin(2, CKPGUID_CLASSID, options.FilterClass);
    block.Pin(3, CKPGUID_BOOL, options.AddToScene != FALSE);
    block.Pin(4, CKPGUID_BOOL, options.ReuseMeshes != FALSE);
    block.Pin(5, CKPGUID_BOOL, options.ReuseMaterials != FALSE);
    block.Setting(0, CKPGUID_BOOL, options.Dynamic != FALSE);
}
} // namespace Detail

#ifdef BML_BEHAVIOR_INTERNAL
inline BlockSpec Make(const Options &options) {
    Blocks::Detail::Definition block(VT_NARRATIVES_OBJECTLOAD);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#else
inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, VT_NARRATIVES_OBJECTLOAD);
    Detail::Define(block, options);
    return std::move(block).Build();
}
#endif


} // namespace ObjectLoad
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_OBJECTLOAD_HPP
