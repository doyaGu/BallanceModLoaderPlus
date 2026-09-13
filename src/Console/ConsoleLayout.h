#ifndef BML_CONSOLE_LAYOUT_H
#define BML_CONSOLE_LAYOUT_H

namespace ConsoleLayout {
    struct Viewport {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct Rect {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct Stack {
        Rect commandBar;
        Rect transientSurface;
        float messageBottom = 0.0f;
    };

    // Computes the Built-in Console bottom stack from measured UI sizes.
    // The command and transient rows remain reserved while hidden, so opening
    // the console or switching transient owners never moves the message board.
    Stack Calculate(Viewport viewport, float commandHeight, float transientHeight) noexcept;
}

#endif // BML_CONSOLE_LAYOUT_H
