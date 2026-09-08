#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include <vulkan/vulkan.h>

namespace vkkk
{

enum class MouseButton : uint8_t {
    Left = 0,
    Right = 1,
    Middle = 2,
};

enum class InputAction : uint8_t {
    Release = 0,
    Press = 1,
    Repeat = 2,
};

namespace input_mod
{

constexpr uint32_t none = 0;
constexpr uint32_t shift = 1u << 0;
constexpr uint32_t ctrl = 1u << 1;
constexpr uint32_t alt = 1u << 2;
constexpr uint32_t super = 1u << 3;

} // namespace input_mod

enum class Key : uint16_t {
    Unknown = 0,
    Space,
    Escape,
    Enter,
    Tab,
    Backspace,
    Insert,
    Delete,
    Right,
    Left,
    Down,
    Up,
    PageUp,
    PageDown,
    Home,
    End,
    CapsLock,
    LeftShift,
    LeftCtrl,
    LeftAlt,
    LeftSuper,
    RightShift,
    RightCtrl,
    RightAlt,
    RightSuper,
    Minus,
    Equal,
    Comma,
    Period,
    Slash,
    Semicolon,
    Apostrophe,
    Grave,
    LeftBracket,
    RightBracket,
    Backslash,
    Digit0,
    Digit1,
    Digit2,
    Digit3,
    Digit4,
    Digit5,
    Digit6,
    Digit7,
    Digit8,
    Digit9,
    A,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,
    Numpad0,
    Numpad1,
    Numpad2,
    Numpad3,
    Numpad4,
    Numpad5,
    Numpad6,
    Numpad7,
    Numpad8,
    Numpad9,
    NumpadEnter,
    NumpadAdd,
    NumpadSubtract,
};

struct InputEvent {
    enum class Kind {
        Key,
        MouseButton,
        MouseMove,
        MouseDrag,
        Scroll,
        Hold,
    };

    Kind kind = Kind::Key;
    Key key = Key::Unknown;
    MouseButton button = MouseButton::Left;
    InputAction action = InputAction::Press;
    uint32_t mods = 0;
    double x = 0.0;
    double y = 0.0;
    double dx = 0.0;
    double dy = 0.0;
    double scroll_x = 0.0;
    double scroll_y = 0.0;
};

// Pointer snapshot in window coordinates, plus sizes for mapping onto a swapchain.
struct InputPointer {
    double x = 0.0;
    double y = 0.0;
    VkExtent2D window_size{};
    VkExtent2D framebuffer_size{};

    bool to_pixel(VkExtent2D target, uint32_t& px, uint32_t& py) const {
        if (window_size.width == 0 || window_size.height == 0
            || target.width == 0 || target.height == 0)
        {
            return false;
        }
        px = std::min(static_cast<uint32_t>(std::floor(
                x * static_cast<double>(target.width) / window_size.width)),
            target.width - 1);
        py = std::min(static_cast<uint32_t>(std::floor(
                y * static_cast<double>(target.height) / window_size.height)),
            target.height - 1);
        return true;
    }
};

inline InputEvent mouse_button_event(MouseButton button, InputAction action,
    double x, double y, uint32_t mods = input_mod::none)
{
    InputEvent event;
    event.kind = InputEvent::Kind::MouseButton;
    event.button = button;
    event.action = action;
    event.mods = mods;
    event.x = x;
    event.y = y;
    return event;
}

} // namespace vkkk
