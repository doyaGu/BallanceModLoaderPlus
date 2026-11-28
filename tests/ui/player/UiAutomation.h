#ifndef BML_UI_AUTOMATION_H
#define BML_UI_AUTOMATION_H

class BMLMod;
class ModContext;

namespace UiAutomation {

void Start(ModContext &runtime, BMLMod &mod);
void OnStartLevel();
void OnBallNavActive();
void AdvanceFrame();
void Shutdown();

} // namespace UiAutomation

#endif // BML_UI_AUTOMATION_H
