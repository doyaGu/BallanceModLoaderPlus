#pragma once

#include <cstdint>

class CKRenderContext;

namespace BML::PlayerTest {

// Writes the Player's current back buffer to a 24-bit BMP file. Both Player
// acceptance Mods capture frames this way, so the DirectX 7 and DirectX 9
// paths stay in one place.
bool SaveRenderFrame(CKRenderContext *render, const char *path,
                     std::uint32_t &version, long &nativeError);

} // namespace BML::PlayerTest
