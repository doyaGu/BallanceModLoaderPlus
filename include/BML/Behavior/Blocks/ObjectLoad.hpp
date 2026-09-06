// The Object Load Building Block: a file, a master name, and what to reuse.
#ifndef BML_BEHAVIOR_BLOCKS_OBJECTLOAD_HPP
#define BML_BEHAVIOR_BLOCKS_OBJECTLOAD_HPP

#include <string>
#include <string_view>

#include "CKAll.h"
#include "BML/Behavior/Detail/Blocks.hpp"
#include "BML/Guids/Narratives.h"

namespace BML::Behavior::Blocks {
namespace ObjectLoad {

struct Options {
    [[nodiscard]] static CKGUID Prototype() noexcept {
        return VT_NARRATIVES_OBJECTLOAD;
    }

    std::string File;
    std::string MasterName;
    CK_CLASSID FilterClass = CKCID_3DOBJECT;
    CKBOOL AddToScene = TRUE;
    CKBOOL ReuseMeshes = TRUE;
    CKBOOL ReuseMaterials = TRUE;
    CKBOOL Dynamic = TRUE;

private:
    template <class Definition>
    void Configure(Definition &block) const {
        block.Pin(0, CKPGUID_STRING, std::string_view(File));
        block.Pin(1, CKPGUID_STRING, std::string_view(MasterName));
        block.Pin(2, CKPGUID_CLASSID, FilterClass);
        block.Pin(3, CKPGUID_BOOL, AddToScene != FALSE);
        block.Pin(4, CKPGUID_BOOL, ReuseMeshes != FALSE);
        block.Pin(5, CKPGUID_BOOL, ReuseMaterials != FALSE);
        block.Setting(0, CKPGUID_BOOL, Dynamic != FALSE);
    }

    friend class BML::Behavior::Detail::BlockAccess;
};

inline Result<Block> Make(const Session &session, const Options &options) {
    BML::Behavior::Detail::Definition block(session, Options::Prototype());
    BML::Behavior::Detail::BlockAccess::Configure(options, block);
    return std::move(block).Build();
}

} // namespace ObjectLoad
} // namespace BML::Behavior::Blocks

#endif // BML_BEHAVIOR_BLOCKS_OBJECTLOAD_HPP
