#ifndef BML_TIME_HPP
#define BML_TIME_HPP

#include "BML/Time.h"

namespace BML::Time {

using Clock = BML_TimeClock;

[[nodiscard]] inline int ReadClock(Clock &out) {
    const BML_TimeInterface *time = FindInterface<BML_TimeInterface>(
        BML_TIME_INTERFACE_ID, BML_TIME_INTERFACE_MAJOR);
    if (!BML_IFACE_HAS(time, BML_TimeInterface, ReadClock))
        return BML_ERROR_NOT_FOUND;
    return time->ReadClock(&out);
}

} // namespace BML::Time

#endif // BML_TIME_HPP
