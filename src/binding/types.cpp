#include "binding/utils.h"
#include "concepts/mesh.h"
#include "vk_ins/types.h"
#include "vk_ins/vkabstraction.h"

using namespace vkkk;

void bind_types(nb::module_& m) {
    nb::class_<VkWrappedInstance> incl(m, "VkInstance");

    incl.def(nb::init<>())
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

    nb::enum_<VERT_COMP>(m, "VERT_COMP")
        .value("VERTEX", VERT_COMP::VERTEX)
        .value("NORMAL", VERT_COMP::NORMAL)
        .value("UV", VERT_COMP::UV)
        .value("COLOR", VERT_COMP::COLOR)
        .export_values();

    nb::class_<Mesh> mecl(m, "Mesh");

    mecl.def(nb::init<VkWrappedInstance*, const std::vector<VERT_COMP>&, bool>())
        .def(nb::init<const Mesh&>())
        //.def("load", )
        .def("unload", &Mesh::unload)
        .def("set_view", &Mesh::set_view)
        .def("load_gpu", &Mesh::load_gpu)
        .def("unload_gpu", &Mesh::unload_gpu)
        .def("get_vert", [](const Mesh& m, size_t idx) {
            if (idx >= m.vcnt * 3)
                throw nb::index_error();
            return m.vbuf[idx];
        });
}