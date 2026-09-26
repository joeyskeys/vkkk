#include "gui/qt_backend.hpp"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <utility>

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QSet>
#include <QString>
#include <QSplitter>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QWidget>
#include <QVBoxLayout>

#include <vulkan/vulkan.h>

#if defined(_WIN32)
#define NOMINMAX
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#include <vulkan/vulkan_win32.h>
#elif defined(__linux__)
#define VK_USE_PLATFORM_XCB_KHR
#include <vulkan/vulkan_xcb.h>
#include <QtGui/qguiapplication.h>
#ifdef QT_FEATURE_xcb
#include <QtGui/qnativeinterface.h>
#endif
#endif

namespace vkkk
{

namespace
{

VkExtent2D clamp_extent(int width, int height) {
    return VkExtent2D{
        static_cast<uint32_t>(std::max(width, 0)),
        static_cast<uint32_t>(std::max(height, 0))
    };
}

int qt_from_key(Key key) {
    const auto value = static_cast<uint16_t>(key);
    if (value >= static_cast<uint16_t>(Key::A) && value <= static_cast<uint16_t>(Key::Z)) {
        return Qt::Key_A + (value - static_cast<uint16_t>(Key::A));
    }
    if (value >= static_cast<uint16_t>(Key::Digit0) && value <= static_cast<uint16_t>(Key::Digit9)) {
        return Qt::Key_0 + (value - static_cast<uint16_t>(Key::Digit0));
    }
    if (value >= static_cast<uint16_t>(Key::F1) && value <= static_cast<uint16_t>(Key::F12)) {
        return Qt::Key_F1 + (value - static_cast<uint16_t>(Key::F1));
    }
    if (value >= static_cast<uint16_t>(Key::Numpad0) && value <= static_cast<uint16_t>(Key::Numpad9)) {
        return Qt::Key_0 + (value - static_cast<uint16_t>(Key::Numpad0));
    }
    switch (key) {
    case Key::Space: return Qt::Key_Space;
    case Key::Escape: return Qt::Key_Escape;
    case Key::Enter: return Qt::Key_Return;
    case Key::Tab: return Qt::Key_Tab;
    case Key::Backspace: return Qt::Key_Backspace;
    case Key::Insert: return Qt::Key_Insert;
    case Key::Delete: return Qt::Key_Delete;
    case Key::Right: return Qt::Key_Right;
    case Key::Left: return Qt::Key_Left;
    case Key::Down: return Qt::Key_Down;
    case Key::Up: return Qt::Key_Up;
    case Key::PageUp: return Qt::Key_PageUp;
    case Key::PageDown: return Qt::Key_PageDown;
    case Key::Home: return Qt::Key_Home;
    case Key::End: return Qt::Key_End;
    case Key::CapsLock: return Qt::Key_CapsLock;
    case Key::LeftShift:
    case Key::RightShift: return Qt::Key_Shift;
    case Key::LeftCtrl:
    case Key::RightCtrl: return Qt::Key_Control;
    case Key::LeftAlt:
    case Key::RightAlt: return Qt::Key_Alt;
    case Key::LeftSuper:
    case Key::RightSuper: return Qt::Key_Meta;
    case Key::Minus: return Qt::Key_Minus;
    case Key::Equal: return Qt::Key_Equal;
    case Key::Comma: return Qt::Key_Comma;
    case Key::Period: return Qt::Key_Period;
    case Key::Slash: return Qt::Key_Slash;
    case Key::Semicolon: return Qt::Key_Semicolon;
    case Key::Apostrophe: return Qt::Key_Apostrophe;
    case Key::Grave: return Qt::Key_QuoteLeft;
    case Key::LeftBracket: return Qt::Key_BracketLeft;
    case Key::RightBracket: return Qt::Key_BracketRight;
    case Key::Backslash: return Qt::Key_Backslash;
    case Key::NumpadEnter: return Qt::Key_Enter;
    case Key::NumpadAdd: return Qt::Key_Plus;
    case Key::NumpadSubtract: return Qt::Key_Minus;
    default: return 0;
    }
}

int qt_keypad_identity(int key) {
    switch (key) {
    case Qt::Key_Insert: return Qt::Key_0;
    case Qt::Key_End: return Qt::Key_1;
    case Qt::Key_Down: return Qt::Key_2;
    case Qt::Key_PageDown: return Qt::Key_3;
    case Qt::Key_Left: return Qt::Key_4;
    case Qt::Key_Clear: return Qt::Key_5;
    case Qt::Key_Right: return Qt::Key_6;
    case Qt::Key_Home: return Qt::Key_7;
    case Qt::Key_Up: return Qt::Key_8;
    case Qt::Key_PageUp: return Qt::Key_9;
    default: return key;
    }
}

uint32_t mods_from_qt(Qt::KeyboardModifiers qt_mods) {
    uint32_t mods = input_mod::none;
    if (qt_mods.testFlag(Qt::ShiftModifier)) {
        mods |= input_mod::shift;
    }
    if (qt_mods.testFlag(Qt::ControlModifier)) {
        mods |= input_mod::ctrl;
    }
    if (qt_mods.testFlag(Qt::AltModifier)) {
        mods |= input_mod::alt;
    }
    if (qt_mods.testFlag(Qt::MetaModifier)) {
        mods |= input_mod::super;
    }
    return mods;
}

} // namespace

class QtVulkanWindow final : public QWindow {
public:
    bool* resize_flag = nullptr;
    QWidget* viewport_container = nullptr;
    bool buttons[3] = {};
    QSet<int> keys;
    QSet<int> keypad_keys;
    float scroll_delta = 0.0f;

    explicit QtVulkanWindow() {
        setSurfaceType(QSurface::VulkanSurface);
    }

    void cursor_position(double& x, double& y) const {
        const QPoint pos = mapFromGlobal(QCursor::pos());
        x = static_cast<double>(pos.x());
        y = static_cast<double>(pos.y());
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            const auto* key_event = static_cast<QKeyEvent*>(event);
            set_key(key_event->key(), event->type() == QEvent::KeyPress,
                key_event->modifiers());
        }
        else if (event->type() == QEvent::Wheel && watched != this
            && is_viewport_wheel_target(watched))
        {
            // Wheel events on the embedded window container do not always
            // reach QWindow::wheelEvent. Capture only those viewport-owned
            // events so dock panels keep independent zoom.
            const auto* wheel = static_cast<QWheelEvent*>(event);
            scroll_delta += static_cast<float>(wheel->angleDelta().y()) / 120.0f;
        }
        return QWindow::eventFilter(watched, event);
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWindow::resizeEvent(event);
        if (resize_flag != nullptr) {
            *resize_flag = true;
        }
    }

    void mousePressEvent(QMouseEvent* event) override {
        set_button(event->button(), true);
        QWindow::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        set_button(event->button(), false);
        QWindow::mouseReleaseEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override {
        set_key(event->key(), true, event->modifiers());
        QWindow::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent* event) override {
        set_key(event->key(), false, event->modifiers());
        QWindow::keyReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override {
        scroll_delta += static_cast<float>(event->angleDelta().y()) / 120.0f;
        QWindow::wheelEvent(event);
    }

private:
    bool is_viewport_wheel_target(const QObject* watched) const {
        if (viewport_container == nullptr || watched == nullptr) {
            return false;
        }
        const auto* widget = qobject_cast<const QWidget*>(watched);
        return widget != nullptr
            && (widget == viewport_container
                || viewport_container->isAncestorOf(widget));
    }
    void set_button(Qt::MouseButton button, bool pressed) {
        if (button == Qt::LeftButton) {
            buttons[0] = pressed;
        }
        else if (button == Qt::RightButton) {
            buttons[1] = pressed;
        }
        else if (button == Qt::MiddleButton) {
            buttons[2] = pressed;
        }
    }

    void set_key(int key, bool pressed, Qt::KeyboardModifiers modifiers) {
        // Some platforms report keypad Enter without KeypadModifier.
        const bool keypad = modifiers.testFlag(Qt::KeypadModifier)
            || key == Qt::Key_Enter;
        const int identity = keypad ? qt_keypad_identity(key) : key;
        if (pressed) {
            (keypad ? keypad_keys : keys).insert(identity);
        }
        else {
            (keypad ? keypad_keys : keys).remove(identity);
        }
    }
};

class QtPanelFrame final : public QFrame {
public:
    explicit QtPanelFrame(const QString& title, QWidget* parent = nullptr)
        : QFrame(parent)
    {
        setFrameShape(QFrame::StyledPanel);
        auto* outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->setSpacing(0);

        auto* title_bar = new QFrame(this);
        title_bar->setObjectName(QStringLiteral("vkkk.panel.title"));
        auto* title_layout = new QHBoxLayout(title_bar);
        title_layout->setContentsMargins(10, 5, 10, 5);
        title_label = new QLabel(title, title_bar);
        title_layout->addWidget(title_label);
        outer->addWidget(title_bar, 0);

        content_layout = new QVBoxLayout();
        content_layout->setContentsMargins(0, 0, 0, 0);
        outer->addLayout(content_layout, 1);
    }

    void set_title(const QString& title) {
        title_label->setText(title);
    }

    void set_content(QWidget* content) {
        if (content == content_widget) {
            return;
        }
        if (content_widget != nullptr) {
            content_widget->setParent(nullptr);
            delete content_widget;
        }
        content_widget = content;
        if (content_widget != nullptr) {
            content_widget->setParent(this);
            content_layout->addWidget(content_widget);
            content_widget->show();
        }
    }

    QWidget* content() const {
        return content_widget;
    }

private:
    QLabel* title_label = nullptr;
    QVBoxLayout* content_layout = nullptr;
    QWidget* content_widget = nullptr;
};

class QtMainWindow final : public QWidget {
public:
    bool closing = false;

    QtMainWindow(int width, int height, const char* title) {
        setWindowTitle(title != nullptr ? title : "vkkk");
        resize(width, height);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);

        main_splitter = new QSplitter(Qt::Horizontal, this);
        main_splitter->setChildrenCollapsible(false);
        right_splitter = new QSplitter(Qt::Vertical, main_splitter);
        right_splitter->setChildrenCollapsible(false);

        auto* viewport_placeholder = new QWidget(main_splitter);
        main_splitter->addWidget(viewport_placeholder);
        main_splitter->addWidget(right_splitter);
        main_splitter->setStretchFactor(0, 7);
        main_splitter->setStretchFactor(1, 3);

        hud_panel = new QtPanelFrame(QStringLiteral("HUD"), right_splitter);
        right_splitter->addWidget(hud_panel);

        status_label = new QLabel(QStringLiteral("Vulkan"), this);
        status_label->setContentsMargins(8, 3, 8, 3);
        layout->addWidget(main_splitter, 1);
        layout->addWidget(status_label, 0);

        QObject::connect(main_splitter, &QSplitter::splitterMoved,
            this, [this](int, int) {
                if (!redrawing) {
                    update_ratio_from_splitter();
                }
            });
        redraw_layout();
    }

    void set_viewport(QWidget* viewport) {
        if (viewport == nullptr || viewport == viewport_widget) {
            return;
        }
        QWidget* previous = main_splitter->replaceWidget(0, viewport);
        if (previous != nullptr && previous != viewport) {
            previous->deleteLater();
        }
        viewport_widget = viewport;
        redraw_layout();
    }

    int add_dock_panel(QWidget* panel, const char* title,
        Qt::DockWidgetArea area)
    {
        if (panel == nullptr || area != Qt::RightDockWidgetArea) {
            return -1;
        }
        auto* frame = new QtPanelFrame(
            title != nullptr ? QString::fromUtf8(title)
                             : QStringLiteral("Panel"),
            right_splitter);
        frame->set_content(panel);
        right_splitter->addWidget(frame);
        panel_frames.push_back(frame);
        frame->show();
        return 0;
    }

    int set_hud_panel(QWidget* panel, const char* title) {
        if (panel == nullptr || hud_panel == nullptr) {
            return -1;
        }
        hud_panel->set_title(
            title != nullptr ? QString::fromUtf8(title)
                             : QStringLiteral("Properties"));
        hud_panel->set_content(panel);
        hud_panel->show();
        return 0;
    }

    QWidget* hud_panel_widget() const {
        return hud_panel != nullptr ? hud_panel->content() : nullptr;
    }

    void set_status(const QString& text) {
        if (status_label != nullptr) {
            status_label->setText(text);
        }
    }

    void set_right_dock_ratio(double ratio) {
        right_ratio = std::clamp(ratio, 0.10, 0.80);
        redraw_layout();
    }

protected:
    void closeEvent(QCloseEvent* event) override {
        closing = true;
        QWidget::closeEvent(event);
    }

    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        redraw_layout();
    }

private:
    void update_ratio_from_splitter() {
        const auto sizes = main_splitter->sizes();
        if (sizes.size() < 2 || sizes[0] + sizes[1] <= 0) {
            return;
        }
        right_ratio = std::clamp(
            static_cast<double>(sizes[1])
                / static_cast<double>(sizes[0] + sizes[1]),
            0.10, 0.80);
    }

    void redraw_layout() {
        if (redrawing || main_splitter == nullptr
            || main_splitter->width() <= 0)
        {
            return;
        }
        redrawing = true;
        const bool was_enabled = updatesEnabled();
        setUpdatesEnabled(false);
        const int total = main_splitter->width();
        const int right = std::max(
            1, static_cast<int>(total * right_ratio));
        main_splitter->setSizes({std::max(1, total - right), right});
        setUpdatesEnabled(was_enabled);
        redrawing = false;
        update();
    }

    QSplitter* main_splitter = nullptr;
    QSplitter* right_splitter = nullptr;
    QWidget* viewport_widget = nullptr;
    QtPanelFrame* hud_panel = nullptr;
    QLabel* status_label = nullptr;
    std::vector<QtPanelFrame*> panel_frames;
    double right_ratio = 0.30;
    bool redrawing = false;
};

QtBackend::QtBackend(int width, int height, const char* title) {
    if (QApplication::instance() == nullptr) {
        static int argc = 1;
        static char arg0[] = "vkkk";
        static char* argv[] = { arg0, nullptr };
        owned_app = new QApplication(argc, argv);
    }

    main = new QtMainWindow(width, height, title);

    surface_window = new QtVulkanWindow();
    surface_window->resize(width, height);
    surface_window->create();
    if (auto* application = QCoreApplication::instance()) {
        // ControlMap polls edge transitions after processEvents(). Install
        // the filter on the application so key presses from dock widgets are
        // visible to the window-level input state as well as the viewport.
        application->installEventFilter(surface_window);
    }

    viewport_root = new QWidget(main);
    auto* viewport_layout = new QVBoxLayout(viewport_root);
    viewport_layout->setContentsMargins(0, 0, 0, 0);

    container = QWidget::createWindowContainer(surface_window, viewport_root);
    container->setFocusPolicy(Qt::StrongFocus);
    surface_window->viewport_container = viewport_root;
    viewport_layout->addWidget(container);
    main->set_viewport(viewport_root);
    container->setFocus();

    main->show();
}

QtBackend::~QtBackend() {
    if (auto* application = QCoreApplication::instance()) {
        application->removeEventFilter(surface_window);
    }
    if (surface_window != nullptr) {
        surface_window->viewport_container = nullptr;
    }
    surface_window = nullptr;
    container = nullptr;
    viewport_root = nullptr;
    delete main;
    main = nullptr;
    delete owned_app;
    owned_app = nullptr;
}

QWidget* QtBackend::main_window() const {
    return main;
}

QWindow* QtBackend::vulkan_window() const {
    return surface_window;
}

QWidget* QtBackend::viewport_panel() const {
    return viewport_root;
}

int QtBackend::add_dock_panel(
    QWidget* panel, const char* title, Qt::DockWidgetArea area)
{
    if (main == nullptr || panel == nullptr) {
        return -1;
    }
    return main->add_dock_panel(panel, title, area);
}

int QtBackend::set_hud_panel(QWidget* panel, const char* title) {
    if (main == nullptr || panel == nullptr) {
        return -1;
    }
    return main->set_hud_panel(panel, title);
}

QWidget* QtBackend::hud_panel() const {
    return main != nullptr ? main->hud_panel_widget() : nullptr;
}

void QtBackend::set_right_dock_ratio(double ratio) {
    if (main != nullptr) {
        main->set_right_dock_ratio(ratio);
    }
}

void QtBackend::set_status(const std::string& text) {
    if (main != nullptr) {
        main->set_status(QString::fromStdString(text));
    }
}

std::vector<const char*> QtBackend::instance_extensions(bool enable_validation) const {
    std::vector<const char*> extensions;
    extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(_WIN32)
    extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__APPLE__)
    extensions.push_back(VK_EXT_METAL_SURFACE_EXTENSION_NAME);
#else
    extensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
    if (enable_validation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VkSurfaceKHR QtBackend::create_surface(VkInstance instance) {
    if (surface_window == nullptr) {
        throw std::runtime_error("Qt Vulkan window is not created");
    }
    surface_window->create();

#if defined(_WIN32)
    VkWin32SurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    info.hinstance = GetModuleHandle(nullptr);
    info.hwnd = reinterpret_cast<HWND>(surface_window->winId());
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateWin32SurfaceKHR(instance, &info, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create Win32 Vulkan surface");
    }
    return surface;
#elif defined(__linux__) && defined(QT_FEATURE_xcb)
    auto* x11 = qApp->nativeInterface<QNativeInterface::QX11Application>();
    if (x11 == nullptr) {
        throw std::runtime_error("Qt X11 native interface is not available");
    }
    VkXcbSurfaceCreateInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
    info.connection = x11->connection();
    info.window = static_cast<xcb_window_t>(surface_window->winId());
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (vkCreateXcbSurfaceKHR(instance, &info, nullptr, &surface) != VK_SUCCESS) {
        throw std::runtime_error("failed to create XCB Vulkan surface");
    }
    return surface;
#else
    (void)instance;
    throw std::runtime_error("Qt Vulkan surface is not implemented on this platform");
#endif
}

VkExtent2D QtBackend::framebuffer_size() const {
    if (surface_window == nullptr) {
        return {};
    }
    const qreal dpr = surface_window->devicePixelRatio();
    return clamp_extent(
        static_cast<int>(surface_window->width() * dpr),
        static_cast<int>(surface_window->height() * dpr));
}

VkExtent2D QtBackend::window_size() const {
    if (surface_window == nullptr) {
        return {};
    }
    return clamp_extent(surface_window->width(), surface_window->height());
}

void QtBackend::wait_until_visible() {
    while (!should_close()) {
        const VkExtent2D size = framebuffer_size();
        if (size.width > 0 && size.height > 0) {
            return;
        }
        QCoreApplication::processEvents();
    }
}

bool QtBackend::should_close() const {
    return main == nullptr || main->closing || !main->isVisible();
}

void QtBackend::poll_events() {
    QCoreApplication::processEvents();
}

void QtBackend::set_resize_flag(bool* resized) {
    if (surface_window != nullptr) {
        surface_window->resize_flag = resized;
    }
}

void* QtBackend::native_handle() const {
    return surface_window != nullptr ? reinterpret_cast<void*>(surface_window->winId()) : nullptr;
}

InputPointer QtBackend::pointer() const {
    InputPointer state;
    if (surface_window != nullptr) {
        surface_window->cursor_position(state.x, state.y);
    }
    state.window_size = window_size();
    state.framebuffer_size = framebuffer_size();
    return state;
}

bool QtBackend::mouse_down(MouseButton button) const {
    if (surface_window == nullptr) {
        return false;
    }
    const auto index = static_cast<uint8_t>(button);
    if (index > 2) {
        return false;
    }
    return surface_window->buttons[index];
}

bool QtBackend::key_down(Key key) const {
    if (surface_window == nullptr) {
        return false;
    }
    const int qt_key = qt_from_key(key);
    if (qt_key == 0) {
        return false;
    }
    const auto value = static_cast<uint16_t>(key);
    if (value >= static_cast<uint16_t>(Key::Numpad0)
        && value <= static_cast<uint16_t>(Key::Numpad9))
    {
        return surface_window->keypad_keys.contains(qt_key);
    }
    if (key == Key::NumpadEnter || key == Key::NumpadAdd
        || key == Key::NumpadSubtract)
    {
        return surface_window->keypad_keys.contains(qt_key);
    }
    return surface_window->keys.contains(qt_key);
}

uint32_t QtBackend::modifiers() const {
    return mods_from_qt(QGuiApplication::queryKeyboardModifiers());
}

bool QtBackend::key_pressed(int key) const {
    return surface_window != nullptr && surface_window->keys.contains(key);
}

float QtBackend::take_scroll_delta() {
    if (surface_window == nullptr) {
        return 0.0f;
    }
    const float delta = surface_window->scroll_delta;
    surface_window->scroll_delta = 0.0f;
    return delta;
}

} // namespace vkkk
