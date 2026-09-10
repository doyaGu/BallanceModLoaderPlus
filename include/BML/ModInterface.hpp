// IMod-aware lifecycle adapters for typed provider interfaces.
#ifndef BML_MOD_INTERFACE_HPP
#define BML_MOD_INTERFACE_HPP

#include "BML/IMod.h"
#include "BML/Interface.hpp"

namespace BML::Interfaces {

namespace Detail {

// IMod friends this narrow adapter so the public authoring layer can preserve
// the exact dependency-registration status instead of collapsing it to bool.
class DependencyAccess final {
public:
    [[nodiscard]] static int Add(IMod &mod, const char *providerModId,
                                 const BMLVersion &minimumVersion, bool optional) noexcept {
        if (!providerModId || !*providerModId)
            return BML_ERROR_INVALID_PARAMETER;
        if (!mod.m_BML)
            return BML_ERROR_FAIL;
        return optional
                   ? mod.m_BML->RegisterOptionalDependency(&mod, providerModId, minimumVersion.major,
                                                           minimumVersion.minor, minimumVersion.patch)
                   : mod.m_BML->RegisterDependency(&mod, providerModId, minimumVersion.major,
                                                   minimumVersion.minor, minimumVersion.patch);
    }
};

template <class Traits>
class DependencyInterface {
public:
    DependencyInterface(const DependencyInterface &) = delete;
    DependencyInterface &operator=(const DependencyInterface &) = delete;
    DependencyInterface(DependencyInterface &&) = delete;
    DependencyInterface &operator=(DependencyInterface &&) = delete;

    // Call Open from OnLoad. Required dependencies have already been checked at
    // that point; optional interfaces legitimately return BML_ERROR_NOT_FOUND.
    [[nodiscard]] int Open() noexcept {
        if (m_DependencyStatus != BML_OK) {
            m_Status = m_DependencyStatus;
            return m_Status;
        }
        m_Status = m_Reference.Open();
        return m_Status;
    }

    void Reset() noexcept {
        m_Reference.Reset();
        m_Status = BML_ERROR_NOT_FOUND;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return static_cast<bool>(m_Reference); }
    [[nodiscard]] bool IsOpen() const noexcept { return m_Reference.IsOpen(); }
    [[nodiscard]] int Status() const noexcept { return m_Status; }
    [[nodiscard]] int DependencyStatus() const noexcept { return m_DependencyStatus; }
    [[nodiscard]] const typename Traits::Interface *Get() const noexcept { return m_Reference.Get(); }
    [[nodiscard]] const typename Traits::Interface *operator->() const noexcept {
        return m_Reference.operator->();
    }
    [[nodiscard]] const typename Traits::Interface &operator*() const noexcept { return *m_Reference; }

protected:
    DependencyInterface(IMod &mod, const char *providerModId, const BMLVersion &minimumVersion,
                        bool optional) noexcept
        : m_DependencyStatus(DependencyAccess::Add(mod, providerModId, minimumVersion, optional)),
          m_Status(m_DependencyStatus) {}

private:
    Reference<Traits> m_Reference;
    int m_DependencyStatus = BML_ERROR_FAIL;
    int m_Status = BML_ERROR_FAIL;
};

} // namespace Detail

template <class Traits>
class RequiredInterface final : public Detail::DependencyInterface<Traits> {
public:
    RequiredInterface(IMod &mod, const char *providerModId,
                      const BMLVersion &minimumVersion = BMLVersion(0, 0, 0)) noexcept
        : Detail::DependencyInterface<Traits>(mod, providerModId, minimumVersion, false) {}
};

template <class Traits>
class OptionalInterface final : public Detail::DependencyInterface<Traits> {
public:
    OptionalInterface(IMod &mod, const char *providerModId,
                      const BMLVersion &minimumVersion = BMLVersion(0, 0, 0)) noexcept
        : Detail::DependencyInterface<Traits>(mod, providerModId, minimumVersion, true) {}
};

} // namespace BML::Interfaces

#endif // BML_MOD_INTERFACE_HPP
