#include "asset_mgr/light_mgr.h"
#include "asset_mgr/mesh_mgr.h"
#include "binding/utils.h"
#include "concepts/camera.h"
#include "concepts/mesh.h"
#include "vk_ins/cmd_buf.h"
#include "vk_ins/pipeline_mgr.h"
#include "vk_ins/shader_mgr.h"
#include "vk_ins/types.h"
#include "vk_ins/uniform_mgr.h"
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

    nb::class_<UniformMgr> umcl(m, "UniformMgr");

    umcl.def(nb::init<VkWrappedInstance*>())
        .def("free_gpu_resources", &UniformMgr::free_gpu_resources)
        //.def("add_buffer", &UniformMgr::add_buffer)
        //.def("add_texture", &UniformMgr::add_texture)
        //.def("add_cubemap", &UniformMgr::add_cubemap)
        .def("generate_writes", &UniformMgr::generate_writes)
        .def("set_dest_set", &UniformMgr::set_dest_set)
        //.def("find_ubo", &Uniform::find_ubo)
        .def("update_ubos", &UniformMgr::update_ubos);

    nb::class_<ShaderModules> smcl(m, "ShaderModules");

    smcl.def(nb::init<VkWrappedInstance*, UniformMgr*>())
        .def("free_gpu_resources", &ShaderModules::free_gpu_resources)
        //.def("add_module", &ShaderModules::add_module)
        .def("assign_tex_image", &ShaderModules::assign_tex_image)
        .def("alloc_uniforms", &ShaderModules::alloc_uniforms)
        .def("set_attribute_binding", &ShaderModules::set_attribute_binding)
        .def("create_input_descriptions", &ShaderModules::create_input_descriptions)
        .def("create_descriptor_layouts", &ShaderModules::create_descriptor_layouts)
        .def("create_descriptor_pool", &ShaderModules::create_descriptor_pool)
        .def("create_descriptor_set", &ShaderModules::create_descriptor_set)
        .def("valid", &ShaderModules::valid)
        .def("get_stages_count", &ShaderModules::get_stages_count)
        .def("get_binding_description_count", &ShaderModules::get_binding_description_count)
        .def("get_attr_description_count", &ShaderModules::get_attr_description_count);

    nb::class_<Pipeline> ppcl(m, "Pipeline");

    ppcl.def(nb::init<VkWrappedInstance*>())
        .def("free_gpu_resources", &Pipeline::free_gpu_resources)
        .def_ro("uniforms", &Pipeline::uniforms)
        .def_ro("modules", &Pipeline::modules);

    nb::class_<CommandBuffers> cbcl(m, "CommandBuffers");

    cbcl.def(nb::init<VkWrappedInstance*>())
        .def("alloc", &CommandBuffers::alloc);

    nb::class_<PipelineMgr> pycl(m, "PipelineMgr");

    pycl.def_static("Instance", nb::overload_cast<VkWrappedInstance*>(&PipelineMgr::instance<VkWrappedInstance*>))
        .def("free_gpu_resources", &PipelineMgr::free_gpu_resources)
        .def("register_pipeline", &PipelineMgr::register_pipeline)
        .def("create_pipelines", &PipelineMgr::create_pipelines)
        .def("create_descriptor_layouts", &PipelineMgr::create_descriptor_layouts)
        .def("create_input_descriptions", &PipelineMgr::create_input_descriptions)
        .def("create_descriptor_pools", &PipelineMgr::create_descriptor_pools)
        .def("create_descriptor_sets", &PipelineMgr::create_descriptor_sets)
        .def("get_pipeline_by_idx", nb::overload_cast<const uint32_t>(&PipelineMgr::get_pipeline))
        .def("get_pipeline_by_name", nb::overload_cast<const std::string&>(&PipelineMgr::get_pipeline))
        .def("bind", &PipelineMgr::bind);

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
        .def("emit_draw_cmd", nb::overload_cast<CommandBuffers&, const uint32_t,
            PipelineMgr&, const std::string&>(&Mesh::emit_draw_cmd))
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

    nb::class_<Camera> cmcl(m, "Camera");
    cmcl.def(nb::init<>())
        .def(nb::init<glm::mat4, glm::mat4>())
        .def("look_at", &Camera::look_at)
        .def("perspective", &Camera::perspective);
}