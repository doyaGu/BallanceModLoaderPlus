#ifndef BML_BUI_H
#define BML_BUI_H

#include "imgui.h"

#include "CKContext.h"

namespace BUI {
    bool Init(CKContext *context, bool originalPlayer = false);
    void Shutdown(CKContext *context, bool originalPlayer = false);

    void NewFrame();
    void Render();
    void OnRender();
};


#endif // BML_BUI_H
