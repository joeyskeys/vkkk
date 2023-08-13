#include "binding/utils.h"
#include "vk_ins/vkabstraction.h"

using namespace vkkk;

void bind_types(nb::module_& m) {
    nb::class_<VkWrappedInstance> pycl(m, "VkInstance");

    pycl.def(nb::init<>())
        .def(nb::init<uint32_t, uint32_t, const std::string&, const std::string&>())
        .def("list_physical_devices", &VkWrappedInstance::list_physical_devices)
        .def("choose_device", &VkWrappedInstance::choose_device)
        .def("init", &VkWrappedInstance::init)
        .def("init_glfw", &VkWrappedInstance::init_glfw)
        .def("create_resources", &VkWrappedInstance::create_resources)
        .def("create_sync_objects", &VkWrappedInstance::create_sync_objects)
        .def("mainloop", &VkWrappedInstance::mainloop);

    //nb::class_<PipelineMgr> pycl(m, "PipelineMgr");

    //pycl.def();
}