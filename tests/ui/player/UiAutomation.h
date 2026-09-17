#ifndef BML_UI_AUTOMATION_H
#define BML_UI_AUTOMATION_H

class BMLMod;

namespace UiAutomation {

void Start(BMLMod &mod);
void OnStartLevel();
void AdvanceFrame();
void Shutdown();

} // namespace UiAutomation

#endif // BML_UI_AUTOMATION_H
