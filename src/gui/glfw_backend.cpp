#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include "gui/glfw_backend.hpp"

#include <cstdint>
#include <stdexcept>

#include "vk_ins/context.hpp"

#ifdef _WIN32
#include <GLFW/glfw3native.h>
#endif

namespace vkkk
{

namespace
{

int glfw_refcount = 0;

} // namespace

GlfwBackend::GlfwBackend(int width, int height, const char* title, bool resizable) {
    if (glfw_refcount == 0) {
        if (glfwInit() == GLFW_FALSE) {
            throw std::runtime_error("failed to initialize GLFW");
        }
    }
    ++glfw_refcount;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, resizable ? GLFW_TRUE : GLFW_FALSE);

    win = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (win == nullptr) {
        --glfw_refcount;
        if (glfw_refcount == 0) {
            glfwTerminate();
        }
        throw std::runtime_error("failed to create GLFW window");
    }

    glfwSetWindowUserPointer(win, this);
    glfwSetFramebufferSizeCallback(win, framebuffer_size_callback);
}

GlfwBackend::~GlfwBackend() {
    if (win != nullptr) {
        glfwDestroyWindow(win);
        win = nullptr;
    }
    if (glfw_refcount > 0) {
        --glfw_refcount;
        if (glfw_refcount == 0) {
            glfwTerminate();
        }
    }
}

void GlfwBackend::framebuffer_size_callback(GLFWwindow* window, int, int) {
    auto* backend = static_cast<GlfwBackend*>(glfwGetWindowUserPointer(window));
    if (backend != nullptr && backend->resize_flag != nullptr) {
        *backend->resize_flag = true;
    }
}

std::vector<const char*> GlfwBackend::instance_extensions(bool enable_validation) const {
    uint32_t count = 0;
    const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&count);
    std::vector extensions(glfw_extensions, glfw_extensions + count);
    if (enable_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VkSurfaceKHR GlfwBackend::create_surface(VkInstance instance) {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, win, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface");
    }
    return surface;
}

VkExtent2D GlfwBackend::framebuffer_size() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(win, &width, &height);
    return VkExtent2D{
        width < 0 ? 0u : static_cast<uint32_t>(width),
        height < 0 ? 0u : static_cast<uint32_t>(height)
    };
}

VkExtent2D GlfwBackend::window_size() const {
    int width = 0;
    int height = 0;
    glfwGetWindowSize(win, &width, &height);
    return VkExtent2D{
        width < 0 ? 0u : static_cast<uint32_t>(width),
        height < 0 ? 0u : static_cast<uint32_t>(height)
    };
}

void GlfwBackend::wait_until_visible() {
    VkExtent2D size = framebuffer_size();
    while ((size.width == 0 || size.height == 0) && !should_close()) {
        glfwWaitEvents();
        size = framebuffer_size();
    }
}

bool GlfwBackend::should_close() const {
    return glfwWindowShouldClose(win) == GLFW_TRUE;
}

void GlfwBackend::poll_events() {
    glfwPollEvents();
}

void GlfwBackend::set_resize_flag(bool* resized) {
    resize_flag = resized;
}

void* GlfwBackend::native_handle() const {
#ifdef _WIN32
    return glfwGetWin32Window(win);
#else
    return nullptr;
#endif
}

InputPointer GlfwBackend::pointer() const {
    InputPointer state;
    glfwGetCursorPos(win, &state.x, &state.y);
    state.window_size = window_size();
    state.framebuffer_size = framebuffer_size();
    return state;
}

bool GlfwBackend::mouse_down(MouseButton button) const {
    return glfwGetMouseButton(win, glfw_from_mouse(button)) == GLFW_PRESS;
}

bool GlfwBackend::key_down(Key key) const {
    const int glfw_key = glfw_from_key(key);
    if (glfw_key == GLFW_KEY_UNKNOWN) {
        return false;
    }
    return glfwGetKey(win, glfw_key) == GLFW_PRESS;
}

uint32_t GlfwBackend::modifiers() const {
    uint32_t mods = input_mod::none;
    if (glfwGetKey(win, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS
        || glfwGetKey(win, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
    {
        mods |= input_mod::shift;
    }
    if (glfwGetKey(win, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS
        || glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS)
    {
        mods |= input_mod::ctrl;
    }
    if (glfwGetKey(win, GLFW_KEY_LEFT_ALT) == GLFW_PRESS
        || glfwGetKey(win, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS)
    {
        mods |= input_mod::alt;
    }
    if (glfwGetKey(win, GLFW_KEY_LEFT_SUPER) == GLFW_PRESS
        || glfwGetKey(win, GLFW_KEY_RIGHT_SUPER) == GLFW_PRESS)
    {
        mods |= input_mod::super;
    }
    return mods;
}

Key key_from_glfw(int glfw_key) {
    if (glfw_key >= GLFW_KEY_A && glfw_key <= GLFW_KEY_Z) {
        return static_cast<Key>(static_cast<uint16_t>(Key::A) + (glfw_key - GLFW_KEY_A));
    }
    if (glfw_key >= GLFW_KEY_0 && glfw_key <= GLFW_KEY_9) {
        return static_cast<Key>(static_cast<uint16_t>(Key::Digit0) + (glfw_key - GLFW_KEY_0));
    }
    if (glfw_key >= GLFW_KEY_F1 && glfw_key <= GLFW_KEY_F12) {
        return static_cast<Key>(static_cast<uint16_t>(Key::F1) + (glfw_key - GLFW_KEY_F1));
    }
    if (glfw_key >= GLFW_KEY_KP_0 && glfw_key <= GLFW_KEY_KP_9) {
        return static_cast<Key>(static_cast<uint16_t>(Key::Numpad0) + (glfw_key - GLFW_KEY_KP_0));
    }
    switch (glfw_key) {
    case GLFW_KEY_SPACE: return Key::Space;
    case GLFW_KEY_ESCAPE: return Key::Escape;
    case GLFW_KEY_ENTER: return Key::Enter;
    case GLFW_KEY_TAB: return Key::Tab;
    case GLFW_KEY_BACKSPACE: return Key::Backspace;
    case GLFW_KEY_INSERT: return Key::Insert;
    case GLFW_KEY_DELETE: return Key::Delete;
    case GLFW_KEY_RIGHT: return Key::Right;
    case GLFW_KEY_LEFT: return Key::Left;
    case GLFW_KEY_DOWN: return Key::Down;
    case GLFW_KEY_UP: return Key::Up;
    case GLFW_KEY_PAGE_UP: return Key::PageUp;
    case GLFW_KEY_PAGE_DOWN: return Key::PageDown;
    case GLFW_KEY_HOME: return Key::Home;
    case GLFW_KEY_END: return Key::End;
    case GLFW_KEY_CAPS_LOCK: return Key::CapsLock;
    case GLFW_KEY_LEFT_SHIFT: return Key::LeftShift;
    case GLFW_KEY_LEFT_CONTROL: return Key::LeftCtrl;
    case GLFW_KEY_LEFT_ALT: return Key::LeftAlt;
    case GLFW_KEY_LEFT_SUPER: return Key::LeftSuper;
    case GLFW_KEY_RIGHT_SHIFT: return Key::RightShift;
    case GLFW_KEY_RIGHT_CONTROL: return Key::RightCtrl;
    case GLFW_KEY_RIGHT_ALT: return Key::RightAlt;
    case GLFW_KEY_RIGHT_SUPER: return Key::RightSuper;
    case GLFW_KEY_MINUS: return Key::Minus;
    case GLFW_KEY_EQUAL: return Key::Equal;
    case GLFW_KEY_COMMA: return Key::Comma;
    case GLFW_KEY_PERIOD: return Key::Period;
    case GLFW_KEY_SLASH: return Key::Slash;
    case GLFW_KEY_SEMICOLON: return Key::Semicolon;
    case GLFW_KEY_APOSTROPHE: return Key::Apostrophe;
    case GLFW_KEY_GRAVE_ACCENT: return Key::Grave;
    case GLFW_KEY_LEFT_BRACKET: return Key::LeftBracket;
    case GLFW_KEY_RIGHT_BRACKET: return Key::RightBracket;
    case GLFW_KEY_BACKSLASH: return Key::Backslash;
    case GLFW_KEY_KP_ENTER: return Key::NumpadEnter;
    case GLFW_KEY_KP_ADD: return Key::NumpadAdd;
    case GLFW_KEY_KP_SUBTRACT: return Key::NumpadSubtract;
    default: return Key::Unknown;
    }
}

int glfw_from_key(Key key) {
    const auto value = static_cast<uint16_t>(key);
    if (value >= static_cast<uint16_t>(Key::A) && value <= static_cast<uint16_t>(Key::Z)) {
        return GLFW_KEY_A + (value - static_cast<uint16_t>(Key::A));
    }
    if (value >= static_cast<uint16_t>(Key::Digit0) && value <= static_cast<uint16_t>(Key::Digit9)) {
        return GLFW_KEY_0 + (value - static_cast<uint16_t>(Key::Digit0));
    }
    if (value >= static_cast<uint16_t>(Key::F1) && value <= static_cast<uint16_t>(Key::F12)) {
        return GLFW_KEY_F1 + (value - static_cast<uint16_t>(Key::F1));
    }
    if (value >= static_cast<uint16_t>(Key::Numpad0) && value <= static_cast<uint16_t>(Key::Numpad9)) {
        return GLFW_KEY_KP_0 + (value - static_cast<uint16_t>(Key::Numpad0));
    }
    switch (key) {
    case Key::Space: return GLFW_KEY_SPACE;
    case Key::Escape: return GLFW_KEY_ESCAPE;
    case Key::Enter: return GLFW_KEY_ENTER;
    case Key::Tab: return GLFW_KEY_TAB;
    case Key::Backspace: return GLFW_KEY_BACKSPACE;
    case Key::Insert: return GLFW_KEY_INSERT;
    case Key::Delete: return GLFW_KEY_DELETE;
    case Key::Right: return GLFW_KEY_RIGHT;
    case Key::Left: return GLFW_KEY_LEFT;
    case Key::Down: return GLFW_KEY_DOWN;
    case Key::Up: return GLFW_KEY_UP;
    case Key::PageUp: return GLFW_KEY_PAGE_UP;
    case Key::PageDown: return GLFW_KEY_PAGE_DOWN;
    case Key::Home: return GLFW_KEY_HOME;
    case Key::End: return GLFW_KEY_END;
    case Key::CapsLock: return GLFW_KEY_CAPS_LOCK;
    case Key::LeftShift: return GLFW_KEY_LEFT_SHIFT;
    case Key::LeftCtrl: return GLFW_KEY_LEFT_CONTROL;
    case Key::LeftAlt: return GLFW_KEY_LEFT_ALT;
    case Key::LeftSuper: return GLFW_KEY_LEFT_SUPER;
    case Key::RightShift: return GLFW_KEY_RIGHT_SHIFT;
    case Key::RightCtrl: return GLFW_KEY_RIGHT_CONTROL;
    case Key::RightAlt: return GLFW_KEY_RIGHT_ALT;
    case Key::RightSuper: return GLFW_KEY_RIGHT_SUPER;
    case Key::Minus: return GLFW_KEY_MINUS;
    case Key::Equal: return GLFW_KEY_EQUAL;
    case Key::Comma: return GLFW_KEY_COMMA;
    case Key::Period: return GLFW_KEY_PERIOD;
    case Key::Slash: return GLFW_KEY_SLASH;
    case Key::Semicolon: return GLFW_KEY_SEMICOLON;
    case Key::Apostrophe: return GLFW_KEY_APOSTROPHE;
    case Key::Grave: return GLFW_KEY_GRAVE_ACCENT;
    case Key::LeftBracket: return GLFW_KEY_LEFT_BRACKET;
    case Key::RightBracket: return GLFW_KEY_RIGHT_BRACKET;
    case Key::Backslash: return GLFW_KEY_BACKSLASH;
    case Key::NumpadEnter: return GLFW_KEY_KP_ENTER;
    case Key::NumpadAdd: return GLFW_KEY_KP_ADD;
    case Key::NumpadSubtract: return GLFW_KEY_KP_SUBTRACT;
    default: return GLFW_KEY_UNKNOWN;
    }
}

MouseButton mouse_from_glfw(int glfw_button) {
    switch (glfw_button) {
    case GLFW_MOUSE_BUTTON_RIGHT: return MouseButton::Right;
    case GLFW_MOUSE_BUTTON_MIDDLE: return MouseButton::Middle;
    default: return MouseButton::Left;
    }
}

int glfw_from_mouse(MouseButton button) {
    switch (button) {
    case MouseButton::Right: return GLFW_MOUSE_BUTTON_RIGHT;
    case MouseButton::Middle: return GLFW_MOUSE_BUTTON_MIDDLE;
    default: return GLFW_MOUSE_BUTTON_LEFT;
    }
}

InputAction action_from_glfw(int glfw_action) {
    switch (glfw_action) {
    case GLFW_RELEASE: return InputAction::Release;
    case GLFW_REPEAT: return InputAction::Repeat;
    default: return InputAction::Press;
    }
}

uint32_t mods_from_glfw(int glfw_mods) {
    uint32_t mods = input_mod::none;
    if ((glfw_mods & GLFW_MOD_SHIFT) != 0) {
        mods |= input_mod::shift;
    }
    if ((glfw_mods & GLFW_MOD_CONTROL) != 0) {
        mods |= input_mod::ctrl;
    }
    if ((glfw_mods & GLFW_MOD_ALT) != 0) {
        mods |= input_mod::alt;
    }
    if ((glfw_mods & GLFW_MOD_SUPER) != 0) {
        mods |= input_mod::super;
    }
    return mods;
}

GLFWwindow* glfw_window(const Context& ctx) {
    const auto* backend = dynamic_cast<const GlfwBackend*>(ctx.window());
    return backend != nullptr ? backend->glfw_window() : nullptr;
}

} // namespace vkkk
