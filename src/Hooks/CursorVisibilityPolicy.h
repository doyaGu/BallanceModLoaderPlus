#ifndef BML_CURSOR_VISIBILITY_POLICY_H
#define BML_CURSOR_VISIBILITY_POLICY_H

// CKInputManager exposes one cursor bit, but the game and the overlay can both
// request it. Remember the game request so releasing the overlay cannot hide a
// cursor that the game still needs (including for menu hit testing).
class CursorVisibilityPolicy {
public:
    void Reset(bool gameVisible) {
        m_GameVisible = gameVisible;
        m_OverlayVisible = false;
    }

    bool SetGameVisible(bool visible) {
        m_GameVisible = visible;
        return IsVisible();
    }

    bool SetOverlayVisible(bool visible) {
        m_OverlayVisible = visible;
        return IsVisible();
    }

    bool IsVisible() const { return m_GameVisible || m_OverlayVisible; }

private:
    bool m_GameVisible = false;
    bool m_OverlayVisible = false;
};

#endif // BML_CURSOR_VISIBILITY_POLICY_H
