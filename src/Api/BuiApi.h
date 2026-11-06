#ifndef BML_API_BUIAPI_H
#define BML_API_BUIAPI_H

#include "BML/Bui.h"
#include "UI/ImGuiStateRecovery.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class ModContext;
class IMod;
struct ImGuiWindow;

namespace BML::Api {
class BuiApi {
public:
    int Begin(ModContext &, IMod &, const char *, const char *, uint32_t *, uint32_t, uint32_t *);
    int End(ModContext &, IMod &);
    int Block(IMod &);
    int Unblock(IMod &);
    void CleanupOwner(IMod &) noexcept;
    void Reset() noexcept;

private:
    struct Window {
        IMod *Owner = nullptr;
        ImGuiWindow *Native = nullptr;
        int Frame = -1, Depth = 0;
        size_t CallDepth = 0;
        Overlay::ImGuiStateSnapshot Before;
        std::string Name;
    };

    struct BlockShare {
        size_t Count = 0;
    };

    void Trim();
    std::vector<Window> m_Windows;
    size_t m_WindowCount = 0;
    std::unordered_map<IMod *, std::unique_ptr<BlockShare>> m_Blocks;
};

const BML_BuiInterface &BuiInterface();
} // namespace BML::Api
#endif
