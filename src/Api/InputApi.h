#ifndef BML_API_INPUTAPI_H
#define BML_API_INPUTAPI_H

#include "BML/Input.h"
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

class ModContext;
class ModRecord;

namespace BML::Api {
class InputApi {
public:
    int ReadKeyboard(ModContext &context, uint32_t flags, BML_InputKeyboardState *outState);
    int AcquireBlock(ModContext &context, ModRecord &owner, uint32_t deviceMask,
                     BML_InputBlockHandle *outBlock);
    int ReleaseBlock(ModContext &context, ModRecord &owner, BML_InputBlockHandle block);
    void CleanupOwner(ModContext &context, ModRecord &owner) noexcept;

private:
    struct Keyboard {
        uint64_t Revision = 0;
        bool Attached = false;
        bool Repetition = false;
        std::array<uint8_t, 256> Bytes{};
        std::array<BML_InputKeyState, 256> Keys{};
        std::vector<BML_InputKeyEvent> Events;
    };

    struct Block {
        ModRecord *Owner = nullptr;
        uint64_t Token = 0;
    };

    std::shared_ptr<Keyboard> m_Raw;
    std::shared_ptr<Keyboard> m_Filtered;
    std::unordered_map<BML_InputBlockHandle, Block> m_Blocks;
};

const BML_InputInterface &InputInterface();
} // namespace BML::Api
#endif
