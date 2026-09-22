#pragma once

class CK3dEntity;
class IBML;

namespace BML::PlayerTest {

// Resolves the ball the shipped composition is currently playing with, through
// the CurrentLevel array the game itself uses. Returns null while the level is
// still loading, so callers retry instead of assuming a fixed object name.
CK3dEntity *ResolveRetailBall(IBML *bml);

} // namespace BML::PlayerTest
