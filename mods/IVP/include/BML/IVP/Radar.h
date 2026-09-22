#ifndef BML_IVP_RADAR_H
#define BML_IVP_RADAR_H

#include "BML/IVP/Types.h"

#include <cstddef>

class IVP_Object;

struct IVP_Radar_Hit {
    IVP_Object *this_object;
    IVP_Object *other_object;
    IVP_DOUBLE dist;
};

// Callback object consumed synchronously by
// IVP_Real_Object::do_radar_checking. Ballance does not retain this pointer.
class IVP_Radar {
public:
    IVP_DOUBLE max_range;
    IVP_DOUBLE max_relative_error;

    virtual void radar_hit(IVP_Radar_Hit *hit) = 0;
};

#if defined(_WIN32) && defined(_MSC_VER)
static_assert(sizeof(IVP_Radar_Hit) == 0x10);
static_assert(offsetof(IVP_Radar_Hit, dist) == 0x08);
static_assert(sizeof(IVP_Radar) == 0x18);
static_assert(offsetof(IVP_Radar, max_range) == 0x08);
static_assert(offsetof(IVP_Radar, max_relative_error) == 0x10);
#endif

#endif // BML_IVP_RADAR_H
