#pragma once

#include <string>
#include <type_traits>
#include <vector>

#include <QMainWindow>
#include <QWidget>
#include <QWindow>

#include "gui/window_backend.hpp"

namespace vkkk
{

class QtVulkanWindow;
class QtMainWindow;

// Qt window/surface backend. GUI chrome is Qt widgets; no ImGui.
class QtBackend : public WindowBackend {
public:
    QtBackend(int width, int height, const char* title);
    ~QtBackend() override;

    QtBackend(const QtBackend&) = delete;
    QtBackend& operator=(const QtBackend&) = delete;

    QWidget* main_window() const;
    QWindow* vulkan_window() const;
    QWidget* viewport_panel() const;
    int add_dock_panel(QWidget* panel, const char* title,
        Qt::DockWidgetArea area = Qt::RightDockWidgetArea);
    int set_hud_panel(QWidget* panel, const char* title);
    QWidget* hud_panel() const;
    void set_right_dock_ratio(double ratio);
    void set_status(const std::string& text);

    std::vector<const char*> instance_extensions(bool enable_validation) const override;
    VkSurfaceKHR create_surface(VkInstance instance) override;
    VkExtent2D framebuffer_size() const override;
    VkExtent2D window_size() const override;
    void wait_until_visible() override;
    bool should_close() const override;
    void poll_events() override;
    void set_resize_flag(bool* resized) override;
    void* native_handle() const override;
    InputPointer pointer() const override;
    bool mouse_down(MouseButton button) const override;
    bool key_down(Key key) const override;
    uint32_t modifiers() const override;
    bool key_pressed(int key) const;
    float take_scroll_delta();

private:
    class QApplication* owned_app = nullptr;
    QtMainWindow* main = nullptr;
    QtVulkanWindow* surface_window = nullptr;
    QWidget* container = nullptr;
    QWidget* viewport_root = nullptr;
};

static_assert(WindowBackendType<QtBackend>);
static_assert(!std::is_abstract_v<QtBackend>);

} // namespace vkkk
