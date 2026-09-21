// Type-safe C++ authoring for ModMenu.h. This layer owns C++ strings, enum
// conversions, exception containment, and best-effort registration cleanup;
// the C header remains the complete stable ABI seam.
#ifndef BML_MOD_MENU_HPP
#define BML_MOD_MENU_HPP

#include "BML/Interface.hpp"
#include "BML/ModMenu.h"

#include <atomic>
#include <cstddef>
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
          m_Description(std::move(description)),
          m_Callbacks(new CallbackState(this)) {}

    virtual ~Page() {
        m_Callbacks->Target.store(nullptr, std::memory_order_release);
        (void) Unregister();
        m_Callbacks->Release();
    }

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
                m_Callbacks,
                &DrawPage,
                &EnterPage,
                &LeavePage,
                &ReleaseCallbacks,
            };

            // RegisterPage takes this reference only when it succeeds.
            m_Callbacks->Retain();
            status = m_Interface->RegisterPage(ownerId, &page);
            if (status == BML_OK) {
                m_OwnerId = std::move(owner);
                m_Registered = true;
            } else {
                m_Callbacks->ReleaseRegistrationReference();
                m_Interface.Reset();
            }
            return status;
        } catch (const std::bad_alloc &) {
            if (!m_Registered)
                m_Callbacks->ReleaseRegistrationReference();
            m_Interface.Reset();
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            if (!m_Registered)
                m_Callbacks->ReleaseRegistrationReference();
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
    struct CallbackState {
        explicit CallbackState(Page *target) noexcept : Target(target) {}

        void Retain() noexcept {
            References.fetch_add(1, std::memory_order_relaxed);
            RegistrationReference.store(true, std::memory_order_release);
        }

        void Release() noexcept {
            if (References.fetch_sub(1, std::memory_order_acq_rel) == 1)
                delete this;
        }

        void ReleaseRegistrationReference() noexcept {
            if (RegistrationReference.exchange(
                    false, std::memory_order_acq_rel)) {
                Release();
            }
        }

        std::atomic<std::size_t> References{1};
        std::atomic_bool RegistrationReference{false};
        std::atomic<Page *> Target;
    };

    class CallbackReference final {
    public:
        explicit CallbackReference(CallbackState *callbacks) noexcept
            : m_Callbacks(callbacks) {
            m_Callbacks->References.fetch_add(1, std::memory_order_relaxed);
        }

        ~CallbackReference() { m_Callbacks->Release(); }

        CallbackReference(const CallbackReference &) = delete;
        CallbackReference &operator=(const CallbackReference &) = delete;

    private:
        CallbackState *m_Callbacks;
    };

    BML_DECLARE_INTERFACE_TRAITS(
        InterfaceTraits, BML_ModMenuInterface, BML_MOD_MENU_INTERFACE_ID,
        BML_MOD_MENU_INTERFACE_MAJOR, UnregisterPage);

    static int BML_CDECL DrawPage(
        void *userData, BML_ModMenuPageFrame *frame) noexcept {
        if (!userData || !frame ||
            frame->StructSize < BML_MOD_MENU_PAGE_FRAME_1_0_SIZE) {
            return BML_ERROR_INVALID_PARAMETER;
        }

        auto *callbacks = static_cast<CallbackState *>(userData);
        CallbackReference reference(callbacks);
        Page *page = callbacks->Target.load(std::memory_order_acquire);
        if (!page)
            return BML_ERROR_NOT_FOUND;
        try {
            const PageAction action = page->OnFrame();
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
        auto *callbacks = static_cast<CallbackState *>(userData);
        CallbackReference reference(callbacks);
        Page *page = callbacks->Target.load(std::memory_order_acquire);
        if (!page)
            return BML_ERROR_NOT_FOUND;
        try {
            page->OnEnter();
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
        auto *callbacks = static_cast<CallbackState *>(userData);
        CallbackReference reference(callbacks);
        Page *page = callbacks->Target.load(std::memory_order_acquire);
        if (!page)
            return BML_ERROR_NOT_FOUND;
        try {
            page->OnLeave(
                static_cast<PageLeaveReason>(reason));
            return BML_OK;
        } catch (const std::bad_alloc &) {
            return BML_ERROR_OUT_OF_MEMORY;
        } catch (...) {
            return BML_ERROR_FAIL;
        }
    }

    static void BML_CDECL ReleaseCallbacks(void *userData) noexcept {
        if (userData) {
            static_cast<CallbackState *>(userData)
                ->ReleaseRegistrationReference();
        }
    }

    Interfaces::Reference<InterfaceTraits> m_Interface;
    std::string m_Id;
    std::string m_Label;
    std::string m_Description;
    std::string m_OwnerId;
    CallbackState *m_Callbacks;
    bool m_Registered = false;
};

} // namespace BML::ModMenu

#endif // BML_MOD_MENU_HPP
