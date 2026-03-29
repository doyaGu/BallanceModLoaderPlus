#ifndef BML_DEVTOOLS_PANEL_H
#define BML_DEVTOOLS_PANEL_H

#include "bml_services.hpp"

namespace devtools {

class Panel {
public:
    virtual ~Panel() = default;

    virtual const char *Id() const = 0;
    virtual const char *Label(const bml::Locale &loc) const = 0;

    virtual void Sample(const bml::ModuleServices &) {}
    virtual void Refresh(const bml::ModuleServices &) {}
    virtual void Draw(const bml::ModuleServices &) = 0;
};

} // namespace devtools

#endif // BML_DEVTOOLS_PANEL_H
