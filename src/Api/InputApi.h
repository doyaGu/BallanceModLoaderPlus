#ifndef BML_API_INPUTAPI_H
#define BML_API_INPUTAPI_H
#include "BML/Input.h"
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>
class ModContext;
class IMod;
namespace BML::Api {
class InputApi {
  public:
    int ReadKeyboard(ModContext &, uint32_t, BML_InputKeyboardState *);
    int AcquireBlock(ModContext &, IMod &, uint32_t, BML_InputBlockHandle *);
    int ReleaseBlock(ModContext &, IMod &, BML_InputBlockHandle);
    void CleanupOwner(ModContext &, IMod &) noexcept;

  private:
    struct Keyboard {
        uint64_t Revision = 0;
        bool Attached = false, Repetition = false;
        std::array<uint8_t, 256> Bytes{};
        std::array<BML_InputKeyState, 256> Keys{};
        std::vector<BML_InputKeyEvent> Events;
    };
    struct Block {
        IMod *Owner = nullptr;
        uint64_t Token = 0;
    };
    std::shared_ptr<Keyboard> m_Raw, m_Filtered;
    std::unordered_map<BML_InputBlockHandle, Block> m_Blocks;
};
const BML_InputInterface &InputInterface();
} // namespace BML::Api
#endif
