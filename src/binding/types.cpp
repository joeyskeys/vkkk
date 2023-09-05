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
    /*************************
     * Necessary vulkan types
     *************************/

    nb::enum_<VkShaderStageFlagBits>(m, "vkShaderStage")
        .value("VERTEX", VK_SHADER_STAGE_VERTEX_BIT)
        .value("TESSELLATION_CONTROL", VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
        .value("TESSELLATION_EVALUATION", VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
        .value("GEOMETRY", VK_SHADER_STAGE_GEOMETRY_BIT)
        .value("FRAGMENT", VK_SHADER_STAGE_FRAGMENT_BIT)
        .value("COMPUTE", VK_SHADER_STAGE_COMPUTE_BIT)
        .export_values();

    nb::enum_<VkPrimitiveTopology>(m, "vkTopology")
        .value("POINT_LIST", VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
        .value("LINE_LIST", VK_PRIMITIVE_TOPOLOGY_LINE_LIST)
        .value("LINE_STRIP", VK_PRIMITIVE_TOPOLOGY_LINE_STRIP)
        .value("TRIANGLE_LIST", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .value("TRIANGLE_FAN", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN)
        .value("LINE_LIST_WITH_ADJACENCY", VK_PRIMITIVE_TOPOLOGY_LINE_LIST_WITH_ADJACENCY)
        .value("LINE_STRIP_WITH_ADJACENCY", VK_PRIMITIVE_TOPOLOGY_LINE_STRIP_WITH_ADJACENCY)
        .value("TRIANGLE_LIST_WITH_ADJACENCY", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY)
        .value("TRIANGLE_STRIP_WITH_ADJACENCY", VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_WITH_ADJACENCY)
        .export_values();

    nb::enum_<VkPolygonMode>(m, "vkPolygonMode")
        .value("FILL", VK_POLYGON_MODE_FILL)
        .value("LINE", VK_POLYGON_MODE_LINE)
        .value("POINT", VK_POLYGON_MODE_POINT)
        .export_values();

    nb::enum_<VkCullModeFlagBits>(m, "vkCullMode")
        .value("NONE", VK_CULL_MODE_NONE)
        .value("FRONT", VK_CULL_MODE_FRONT_BIT)
        .value("BACK", VK_CULL_MODE_BACK_BIT)
        .value("FRONT_AND_BACK", VK_CULL_MODE_FRONT_AND_BACK)
        .export_values();

    nb::enum_<VkFrontFace>(m, "vkFrontFace")
        .value("CCW", VK_FRONT_FACE_COUNTER_CLOCKWISE)
        .value("CW", VK_FRONT_FACE_CLOCKWISE)
        .export_values();

    nb::enum_<VkSampleCountFlagBits>(m, "vkSampleCount")
        .value("_1", VK_SAMPLE_COUNT_1_BIT)
        .value("_2", VK_SAMPLE_COUNT_2_BIT)
        .value("_4", VK_SAMPLE_COUNT_4_BIT)
        .value("_8", VK_SAMPLE_COUNT_8_BIT)
        .value("_16", VK_SAMPLE_COUNT_16_BIT)
        .value("_32", VK_SAMPLE_COUNT_32_BIT)
        .value("_64", VK_SAMPLE_COUNT_64_BIT)
        .export_values();

    nb::enum_<VkCompareOp>(m, "vkCmpOp")
        .value("NEVER", VK_COMPARE_OP_NEVER)
        .value("LESS", VK_COMPARE_OP_LESS)
        .value("EQUAL", VK_COMPARE_OP_EQUAL)
        .value("LESS_OR_EQUAL", VK_COMPARE_OP_LESS_OR_EQUAL)
        .value("GREATER", VK_COMPARE_OP_GREATER)
        .value("NOT_EQUAL", VK_COMPARE_OP_NOT_EQUAL)
        .value("GREATER_OR_EQUAL", VK_COMPARE_OP_GREATER_OR_EQUAL)
        .value("ALWAYS", VK_COMPARE_OP_ALWAYS)
        .export_values();

    // Too much of this shit...
    // Bind the necessary parts for now
    nb::enum_<VkFormat>(m, "vkFormat")
        .value("UNDEFINED", VK_FORMAT_UNDEFINED)
        .value("R4G4_UNORM_PACK8", VK_FORMAT_R4G4_UNORM_PACK8)
        .value("R4G4B4A4_UNORM_PACK16", VK_FORMAT_R4G4B4A4_UNORM_PACK16)
        .value("B4G4R4A4_UNORM_PACK16", VK_FORMAT_B4G4R4A4_UNORM_PACK16)
        .value("R5G6B5_UNORM_PACK16", VK_FORMAT_R5G6B5_UNORM_PACK16)
        .value("B5G6R5_UNORM_PACK16", VK_FORMAT_B5G6R5_UNORM_PACK16)
        .value("R5G5B5A1_UNORM_PACK16", VK_FORMAT_R5G5B5A1_UNORM_PACK16)
        .value("B5G5R5A1_UNORM_PACK16", VK_FORMAT_B5G5R5A1_UNORM_PACK16)
        .value("A1R5G5B5_UNORM_PACK16", VK_FORMAT_A1R5G5B5_UNORM_PACK16)
        .value("R8_UNORM", VK_FORMAT_R8_UNORM)
        .value("R8_SNORM", VK_FORMAT_R8_SNORM)
        .value("R8_USCALED", VK_FORMAT_R8_USCALED)
        .value("R8_SSCALED", VK_FORMAT_R8_SSCALED)
        .value("R8_UINT", VK_FORMAT_R8_UINT)
        .value("R8_SINT", VK_FORMAT_R8_SINT)
        .value("R8_SRGB", VK_FORMAT_R8_SRGB)
        .value("R8G8_UNORM", VK_FORMAT_R8G8_UNORM)
        .value("R8G8_SNORM", VK_FORMAT_R8G8_SNORM)
        .value("R8G8_USCALED", VK_FORMAT_R8G8_USCALED)
        .value("R8G8_SSCALED", VK_FORMAT_R8G8_SSCALED)
        .value("R8G8_UINT", VK_FORMAT_R8G8_UINT)
        .value("R8G8_SINT", VK_FORMAT_R8G8_SINT)
        .value("R8G8_SRGB", VK_FORMAT_R8G8_SRGB)
        .value("R8G8B8_UNORM", VK_FORMAT_R8G8B8_UNORM)
        .value("R8G8B8_SNORM", VK_FORMAT_R8G8B8_SNORM)
        .value("R8G8B8_USCALED", VK_FORMAT_R8G8B8_USCALED)
        .value("R8G8B8_SSCALED", VK_FORMAT_R8G8B8_SSCALED)
        .value("R8G8B8_UINT", VK_FORMAT_R8G8B8_UINT)
        .value("R8G8B8_SINT", VK_FORMAT_R8G8B8_SINT)
        .value("R8G8B8_SRGB", VK_FORMAT_R8G8B8_SRGB)
        .value("R8G8B8A8_UNORM", VK_FORMAT_R8G8B8A8_UNORM)
        .value("R8G8B8A8_SNORM", VK_FORMAT_R8G8B8A8_SNORM)
        .value("R8G8B8A8_USCALED", VK_FORMAT_R8G8B8A8_USCALED)
        .value("R8G8B8A8_SSCALED", VK_FORMAT_R8G8B8A8_SSCALED)
        .value("R8G8B8A8_UINT", VK_FORMAT_R8G8B8A8_UINT)
        .value("R8G8B8A8_SINT", VK_FORMAT_R8G8B8A8_SINT)
        .value("R8G8B8A8_SRGB", VK_FORMAT_R8G8B8A8_SRGB)
        .export_values();

    /*************************
     * Abstraction types
     *************************/

    nb::enum_<VERT_COMP>(m, "VERT_COMP")
        .value("VERTEX", VERT_COMP::VERTEX)
        .value("NORMAL", VERT_COMP::NORMAL)
        .value("UV", VERT_COMP::UV)
        .value("COLOR", VERT_COMP::COLOR)
        .export_values();

    nb::class_<ShaderModule> smcl(m, "ShaderModule");
    
    smcl.def(nb::init<>())
        .def("load", [](ShaderModule& m, const std::string& path, VkShaderStageFlagBits stage) {
            return m.load(path, stage);
        })
        .def("get_uniform_info", &ShaderModule::get_uniform_info);

    nb::class_<PipelineOption>(m, "PipelineOption")
        .def(nb::init<>())
        .def("setup_input_assembly", &PipelineOption::setup_input_assembly)
        .def("setup_viewport", &PipelineOption::setup_viewport)
        .def("setup_scissor", &PipelineOption::setup_scissor)
        .def("setup_rasterizer", &PipelineOption::setup_rasterizer)
        .def("setup_multisampling", &PipelineOption::setup_multisampling)
        .def("setup_depth_stencil", &PipelineOption::setup_depth_stencil);

    nb::class_<VkWrappedInstance> incl(m, "VkInstance");

    incl.def(nb::init<>())
        .def(nb::init<uint32_t, uint32_t, const std::string&, const std::string&>())
        .def("setup_resolution", &VkWrappedInstance::setup_resolution)
        .def("list_physical_devices", &VkWrappedInstance::list_physical_devices)
        .def("choose_device", &VkWrappedInstance::choose_device)
        .def("init", &VkWrappedInstance::init)
        .def("init_glfw", &VkWrappedInstance::init_glfw)
        .def("create_logical_device", &VkWrappedInstance::create_logical_device)
        .def("create_renderpass", &VkWrappedInstance::create_renderpass)
        .def("create_command_pool", &VkWrappedInstance::create_command_pool)
        .def("create_color_resource", &VkWrappedInstance::create_color_resource)
        .def("create_depth_resource", &VkWrappedInstance::create_depth_resource)
        .def("create_framebuffer_from_target", &VkWrappedInstance::create_framebuffer_from_target)
        .def("create_framebuffer_from_swapchain_target", &VkWrappedInstance::create_framebuffer_from_swapchain_target)
        .def("create_resources", &VkWrappedInstance::create_resources)
        .def("create_sync_objects", &VkWrappedInstance::create_sync_objects)
        .def("mainloop", &VkWrappedInstance::mainloop)
        .def("get_image_buffer", &VkWrappedInstance::get_image_buffer)
        .def("create_pipeline", &VkWrappedInstance::create_pipeline);

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

    nb::class_<ShaderModulesDeprecated> smdcl(m, "ShaderModulesDeprecated");

    smdcl.def(nb::init<VkWrappedInstance*, UniformMgr*>())
        .def("free_gpu_resources", &ShaderModulesDeprecated::free_gpu_resources)
        //.def("add_module", &ShaderModulesDeprecated::add_module)
        .def("assign_tex_image", &ShaderModulesDeprecated::assign_tex_image)
        .def("alloc_uniforms", &ShaderModulesDeprecated::alloc_uniforms)
        .def("set_attribute_binding", &ShaderModulesDeprecated::set_attribute_binding)
        .def("create_input_descriptions", &ShaderModulesDeprecated::create_input_descriptions)
        .def("create_descriptor_layouts", &ShaderModulesDeprecated::create_descriptor_layouts)
        .def("create_descriptor_pool", &ShaderModulesDeprecated::create_descriptor_pool)
        .def("create_descriptor_set", &ShaderModulesDeprecated::create_descriptor_set)
        .def("valid", &ShaderModulesDeprecated::valid)
        .def("get_stages_count", &ShaderModulesDeprecated::get_stages_count)
        .def("get_binding_description_count", &ShaderModulesDeprecated::get_binding_description_count)
        .def("get_attr_description_count", &ShaderModulesDeprecated::get_attr_description_count);

    nb::class_<PipelineDeprecated> ppcl(m, "Pipeline");

    ppcl.def(nb::init<VkWrappedInstance*>())
        .def("free_gpu_resources", &PipelineDeprecated::free_gpu_resources)
        .def_ro("uniforms", &PipelineDeprecated::uniforms)
        .def_ro("modules", &PipelineDeprecated::modules);

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
        .def("load", [](MeshMgr& mgr, const std::vector<VERT_COMP>& cs, const uint32_t v, nb::bytes& vbuf, const uint32_t i, nb::bytes& ibuf) {
            mgr.load(cs, v, vbuf.c_str(), vbuf.size(), i, ibuf.c_str(), ibuf.size());
        })
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