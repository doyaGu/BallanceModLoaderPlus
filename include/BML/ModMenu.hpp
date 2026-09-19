// Type-safe C++ authoring for ModMenu.h. This layer owns C++ strings, enum
// conversions, exception containment, and best-effort registration cleanup;
// the C header remains the complete stable ABI seam.
#ifndef BML_MOD_MENU_HPP
#define BML_MOD_MENU_HPP

#include "BML/Interface.hpp"
#include "BML/ModMenu.h"

#include <new>
#include <string>
#include <utility>

namespace BML::ModMenu {

enum class PageAction {
    None = BML_MOD_MENU_PAGE_NONE,
    Back = BML_MOD_MENU_PAGE_BACK,
    Close = BML_MOD_MENU_PAGE_CLOSE,
};

enum class PageLeaveReason {
    Back = BML_MOD_MENU_PAGE_LEAVE_BACK,
    Close = BML_MOD_MENU_PAGE_LEAVE_CLOSE,
};

// One immediate-mode page contribution. Derive from Page, keep it alive with
// the owning Mod, register it from OnLoad, and unregister it from OnUnload.
// The destructor is a final best-effort fallback; explicit unregistration is
// required because the C interface, including destruction of a registered Page,
// is game-thread-only.
class Page {
public:
    Page(std::string id, std::string label, std::string description = {})
        : m_Id(std::move(id)), m_Label(std::move(label)),
          m_Description(std::move(description)) {}

    virtual ~Page() { (void) Unregister(); }

    Page(const Page &) = delete;
    Page &operator=(const Page &) = delete;
    Page(Page &&) = delete;
    Page &operator=(Page &&) = delete;

    [[nodiscard]] int Register(const char *ownerId = nullptr) noexcept {
        if (m_Registered)
            return BML_ERROR_ALREADY_EXISTS;

        int status = m_Interface.Open();
        if (status != BML_OK)
            return status;

        try {
            std::string owner = ownerId ? ownerId : "";
            const BML_ModMenuPage page = {
                sizeof(BML_ModMenuPage),
                m_Id.c_str(),
                m_Label.c_str(),
                m_Description.c_str(),
                this,
                &DrawPage,
                &EnterPage,
                &LeavePage,
            };
            status = m_Interface->RegisterPage(ownerId, &page);
            if (status == BML_OK) {
                m_OwnerId = std::move(owner);
                m_Registered = true;
            } else {
                m_Interface.Reset();
            }
            return status;
        } catch (const std::bad_alloc &) {
            m_Interface.Reset();
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            m_Interface.Reset();
            return BML_ERROR_FAIL;
        }
    }

    [[nodiscard]] int Unregister() noexcept {
        if (!m_Registered)
            return BML_OK;
        if (!m_Interface)
            return BML_ERROR_NOT_FOUND;

        const char *ownerId = m_OwnerId.empty() ? nullptr : m_OwnerId.c_str();
        const int status = m_Interface->UnregisterPage(ownerId, m_Id.c_str());
        if (status == BML_OK || status == BML_ERROR_NOT_FOUND) {
            m_OwnerId.clear();
            m_Registered = false;
            m_Interface.Reset();
        }
        return status;
    }

    [[nodiscard]] bool IsRegistered() const noexcept { return m_Registered; }
    [[nodiscard]] const std::string &GetId() const noexcept { return m_Id; }
    [[nodiscard]] const std::string &GetLabel() const noexcept { return m_Label; }
    [[nodiscard]] const std::string &GetDescription() const noexcept {
        return m_Description;
    }

protected:
    virtual PageAction OnFrame() = 0;
    virtual void OnEnter() {}
    virtual void OnLeave(PageLeaveReason) {}

private:
    BML_DECLARE_INTERFACE_TRAITS(
        InterfaceTraits, BML_ModMenuInterface, BML_MOD_MENU_INTERFACE_ID,
        BML_MOD_MENU_INTERFACE_MAJOR, UnregisterPage);

    static int BML_CDECL DrawPage(
        void *userData, BML_ModMenuPageFrame *frame) noexcept {
        if (!userData || !frame ||
            frame->StructSize < BML_MOD_MENU_PAGE_FRAME_1_0_SIZE) {
            return BML_ERROR_INVALID_PARAMETER;
        }

        try {
            const PageAction action = static_cast<Page *>(userData)->OnFrame();
            if (action < PageAction::None || action > PageAction::Close)
                return BML_ERROR_MALFORMED_MESSAGE;
            frame->Action = static_cast<BML_ModMenuPageAction>(action);
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return BML_ERROR_FAIL;
        }
    }

    static int BML_CDECL EnterPage(void *userData) noexcept {
        if (!userData)
            return BML_ERROR_INVALID_PARAMETER;
        try {
            static_cast<Page *>(userData)->OnEnter();
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return BML_ERROR_FAIL;
        }
    }

    static int BML_CDECL LeavePage(
        void *userData, BML_ModMenuPageLeaveReason reason) noexcept {
        if (!userData)
            return BML_ERROR_INVALID_PARAMETER;
        try {
            static_cast<Page *>(userData)->OnLeave(
                static_cast<PageLeaveReason>(reason));
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return BML_ERROR_FAIL;
        }
    }

    Interfaces::Reference<InterfaceTraits> m_Interface;
    std::string m_Id;
    std::string m_Label;
    std::string m_Description;
    std::string m_OwnerId;
    bool m_Registered = false;
};

} // namespace BML::ModMenu

#endif // BML_MOD_MENU_HPP
