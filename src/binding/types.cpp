#include "asset_mgr/light_mgr.h"
#include "asset_mgr/mesh_mgr.h"
#include "binding/utils.h"
#include "concepts/mesh.h"
#include "vk_ins/pipeline_mgr.h"
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
        .def("mainloop", &VkWrappedInstance::mainloop)
        .def("get_image_buffer", &VkWrappedInstance::get_image_buffer);

    nb::class_<PipelineMgr> pycl(m, "PipelineMgr");

    pycl.def_static("Instance", nb::overload_cast<VkWrappedInstance*>(&PipelineMgr::instance<VkWrappedInstance*>))
        .def("free_gpu_resources", &PipelineMgr::free_gpu_resources)
        .def("register_pipeline", &PipelineMgr::register_pipeline)
        .def("create_pipelines", &PipelineMgr::create_pipelines)
        .def("create_descriptor_layouts", &PipelineMgr::create_descriptor_layouts)
        .def("create_input_descriptions", &PipelineMgr::create_input_descriptions)
        .def("create_descriptor_pools", &PipelineMgr::create_descriptor_pools)
        .def("create_descriptor_sets", &PipelineMgr::create_descriptor_sets);

    nb::enum_<VERT_COMP>(m, "VERT_COMP")
        .value("VERTEX", VERT_COMP::VERTEX)
        .value("NORMAL", VERT_COMP::NORMAL)
        .value("UV", VERT_COMP::UV)
        .value("COLOR", VERT_COMP::COLOR)
        .export_values();

    nb::class_<Mesh> mecl(m, "Mesh");

    mecl.def(nb::init<VkWrappedInstance*, const std::vector<VERT_COMP>&, bool>())
        .def(nb::init<const Mesh&>())
        .def("load", [](Mesh& m, uint32_t v, nb::bytes& vbuf, uint32_t i, nb::bytes& ibuf) {
            m.load(v, vbuf.c_str(), vbuf.size(), i, ibuf.c_str(), ibuf.size());
        })
        .def("unload", &Mesh::unload)
        .def("set_view", &Mesh::set_view)
        .def("load_gpu", &Mesh::load_gpu)
        .def("unload_gpu", &Mesh::unload_gpu)
        .def("get_vert", [](const Mesh& m, size_t idx) {
            if (idx >= m.vcnt * 3)
                throw nb::index_error();
            return m.vbuf[idx];
        });

    nb::class_<MeshMgr> mmcl(m, "MeshMgr");

    mmcl.def_static("Instance", nb::overload_cast<VkWrappedInstance*>(&MeshMgr::instance<VkWrappedInstance*>))
        .def("load", &MeshMgr::load)
        .def("pour_info_gpu", &MeshMgr::pour_into_gpu)
        .def("free_gpu_resources", &MeshMgr::free_gpu_resources);
        //.def("emit_draw_cmds", &MeshMgr::emit_draw_cmds);

    nb::class_<LightInfo> licl(m, "LightInfo");

    licl.def_rw("pt_lights", &LightInfo::pt_lights)
        .def_rw("dir_lights", &LightInfo::dir_lights)
        .def_rw("spot_lights", &LightInfo::spot_lights);

    nb::class_<LightMgr> lmcl(m, "LightMgr");

    lmcl.def_static("Instance", nb::overload_cast<>(&LightMgr::instance<>))
        .def("add_pt_light", &LightMgr::add_pt_light)
        .def("add_dir_light", &LightMgr::add_dir_light)
        .def("add_spot_light", &LightMgr::add_spot_light);
}