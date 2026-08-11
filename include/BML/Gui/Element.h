// What every BGui element is underneath: a CK2dEntity created in the CKContext and added
// to the current level, set to homogeneous coordinates so its position and size are
// fractions of the window, 0 to 1, with clipping to the camera and ratio offset both
// off. Position and size are read and written through that entity, so what a Mod sets
// here is what the render engine draws next frame.
//
// The entity belongs to the level, which is why an element cannot outlive it: destroy
// the Gui that owns the element while the level is still there, from IMod::OnUnload at
// the latest, and never keep one across a level being torn down. Deleting the element
// destroys the entity.
//
// GetZOrder and SetZOrder place the element inside the game's own 2D layer. An element
// starts at 20, and Panel puts itself at 0 so that a background sits behind the rest.
// Process is what the derived elements use to push their text out every frame; it is
// called by Gui::Process, not by the engine.
#ifndef BML_GUI_ELEMENT_H
#define BML_GUI_ELEMENT_H

#include "Vx2dVector.h"
#include "VxColor.h"
#include "CKDefines.h"

#include "BML/Defines.h"

namespace BGui {
    class BML_EXPORT Element {
        friend class Gui;

    public:
        explicit Element(const char *name);
        virtual ~Element();

        virtual Vx2DVector GetPosition();
        virtual void SetPosition(Vx2DVector pos);

        virtual Vx2DVector GetSize();
        virtual void SetSize(Vx2DVector size);

        virtual int GetZOrder();
        virtual void SetZOrder(int z);

        virtual bool IsVisible();
        virtual void SetVisible(bool visible);

        virtual void Process() {};

    protected:
        CK2dEntity *m_2dEntity;
    };
}

#endif // BML_GUI_ELEMENT_H
