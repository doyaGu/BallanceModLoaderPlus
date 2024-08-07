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
