#include "BML/DataShare.hpp"

#include <utility>

namespace {
void BML_CDECL ReceiveValue(
    const char *, const void *, std::size_t, void *) {}

void BML_CDECL ReleaseValue(const char *, void *) {}
}

int BML_TestDataShareCppFacade() {
    const char value[] = "value";
    BML::DataShare share("cpp-facade");
    BML::DataShare copy = share;
    BML::DataShare moved = std::move(copy);
    if (!moved.Set("key", value, sizeof(value)))
        return 0;

    BML::DataShareRequest request = moved.Request(
        "pending", &ReceiveValue, nullptr, &ReleaseValue);
    BML::DataShareRequest movedRequest = std::move(request);
    (void) movedRequest.Cancel();
    return moved.Has("key") ? 1 : 0;
}
