#include "gui/qt_backend.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <QApplication>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QSet>
#include <QString>
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

} // namespace

class QtVulkanWindow final : public QWindow {
public:
    bool* resize_flag = nullptr;
    bool buttons[3] = {};
    QSet<int> keys;
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
            set_key(key_event->key(), event->type() == QEvent::KeyPress);
        }
        else if (event->type() == QEvent::Wheel) {
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
        set_key(event->key(), true);
        QWindow::keyPressEvent(event);
    }

    void keyReleaseEvent(QKeyEvent* event) override {
        set_key(event->key(), false);
        QWindow::keyReleaseEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override {
        scroll_delta += static_cast<float>(event->angleDelta().y()) / 120.0f;
        QWindow::wheelEvent(event);
    }

private:
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

    void set_key(int key, bool pressed) {
        if (pressed) {
            keys.insert(key);
        }
        else {
            keys.remove(key);
        }
    }
};

class QtMainWindow final : public QMainWindow {
public:
    bool closing = false;

protected:
    void closeEvent(QCloseEvent* event) override {
        closing = true;
        QMainWindow::closeEvent(event);
    }
};

QtBackend::QtBackend(int width, int height, const char* title) {
    if (QApplication::instance() == nullptr) {
        static int argc = 1;
        static char arg0[] = "vkkk";
        static char* argv[] = { arg0, nullptr };
        owned_app = new QApplication(argc, argv);
    }

    main = new QtMainWindow();
    main->setWindowTitle(title != nullptr ? title : "vkkk");
    main->resize(width, height);

    surface_window = new QtVulkanWindow();
    surface_window->resize(width, height);
    surface_window->create();

    container = QWidget::createWindowContainer(surface_window, main);
    container->setFocusPolicy(Qt::StrongFocus);
    container->installEventFilter(surface_window);
    main->setCentralWidget(container);
    container->setFocus();

    hud_dock = new QDockWidget("HUD", main);
    hud_dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    auto* hud_root = new QWidget(hud_dock);
    auto* layout = new QVBoxLayout(hud_root);
    status_label = new QLabel("Vulkan", hud_root);
    status_label->setWordWrap(true);
    layout->addWidget(status_label);
    layout->addStretch(1);
    hud_dock->setWidget(hud_root);
    main->addDockWidget(Qt::RightDockWidgetArea, hud_dock);

    main->show();
}

QtBackend::~QtBackend() {
    surface_window = nullptr;
    container = nullptr;
    hud_dock = nullptr;
    status_label = nullptr;
    delete main;
    main = nullptr;
    delete owned_app;
    owned_app = nullptr;
}

QMainWindow* QtBackend::main_window() const {
    return main;
}

QWindow* QtBackend::vulkan_window() const {
    return surface_window;
}

QWidget* QtBackend::hud_panel() const {
    return hud_dock != nullptr ? hud_dock->widget() : nullptr;
}

void QtBackend::set_status(const std::string& text) {
    if (status_label != nullptr) {
        status_label->setText(QString::fromStdString(text));
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

void QtBackend::cursor_position(double& x, double& y) const {
    if (surface_window != nullptr) {
        surface_window->cursor_position(x, y);
        return;
    }
    x = 0.0;
    y = 0.0;
}

bool QtBackend::mouse_pressed(int button) const {
    if (surface_window == nullptr || button < 0 || button > 2) {
        return false;
    }
    return surface_window->buttons[button];
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
