#include "asset_mgr/light_mgr.h"
#include "asset_mgr/drawable_mgr.h"
#include "binding/utils.h"
#include "concepts/camera.h"
#include "concepts/line.h"
#include "concepts/mesh.h"
#include "vk_ins/cmd_buf.h"
#include "vk_ins/pipeline_mgr.h"
#include "vk_ins/shader_mgr.h"
#include "vk_ins/types.h"
#include "vk_ins/uniform_mgr.h"
#include "vk_ins/vkabstraction.h"

#include <new>

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
        .value("D32_SFLOAT", VK_FORMAT_D32_SFLOAT)
        .export_values();

    nb::enum_<VkImageAspectFlagBits>(m, "vkImageAspect")
        .value("ASPECT_COLOR", VK_IMAGE_ASPECT_COLOR_BIT)
        .value("ASPECT_DEPTH", VK_IMAGE_ASPECT_DEPTH_BIT)
        .value("ASPECT_STENCIL", VK_IMAGE_ASPECT_STENCIL_BIT)
        .value("ASPECT_METADATA", VK_IMAGE_ASPECT_METADATA_BIT)
        .export_values();

    nb::enum_<VkImageUsageFlagBits>(m, "vkImageUsage")
        .value("USAGE_TRANSFER_SRC", VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
        .value("USAGE_TRANSFER_DST", VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .value("USAGE_SAMPLED", VK_IMAGE_USAGE_SAMPLED_BIT)
        .value("USAGE_STORAGE", VK_IMAGE_USAGE_STORAGE_BIT)
        .value("USAGE_COLOR_ATTACH", VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        .value("USAGE_DEPTH_STENCIL_ATTACH", VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        .value("USAGE_INPUT_ATTACH", VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT)
        .export_values();

    nb::class_<VkImageUsageFlags>(m, "vkUsage")
        .def_static("xor", [](std::vector<VkImageUsageFlagBits>& l) {
            VkImageUsageFlags usage = 0;
            for (auto& u : l)
                usage |= u;
            return usage;
        });

    nb::enum_<VkAttachmentLoadOp>(m, "vkAttachmentLoadOp")
        .value("LOP_LOAD", VK_ATTACHMENT_LOAD_OP_LOAD)
        .value("LOP_CLEAR", VK_ATTACHMENT_LOAD_OP_CLEAR)
        .value("LOP_DONT_CARE", VK_ATTACHMENT_LOAD_OP_DONT_CARE)
        .value("LOP_NONE", VK_ATTACHMENT_LOAD_OP_NONE_EXT)
        .export_values();

    nb::enum_<VkAttachmentStoreOp>(m, "vkAttachmentStoreOp")
        .value("SOP_STORE", VK_ATTACHMENT_STORE_OP_STORE)
        .value("SOP_DONT_CARE", VK_ATTACHMENT_STORE_OP_DONT_CARE)
        .value("SOP_NONE", VK_ATTACHMENT_STORE_OP_NONE)
        .export_values();

    nb::enum_<VkImageLayout>(m, "vkImageLayout")
        .value("LAYOUT_UNDEFINED", VK_IMAGE_LAYOUT_UNDEFINED)
        .value("LAYOUT_GENERAL", VK_IMAGE_LAYOUT_GENERAL)
        .value("LAYOUT_COLOR_ATTACH_OPTIMAL", VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
        .value("LAYOUT_DEPTH_STENCIL_ATTACH_OPTIMAL", VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
        .value("LAYOUT_DEPTH_STENCIL_READ_OPTIMAL", VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        .value("LAYOUT_SHADER_READ_OPTIMAL", VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .value("LAYOUT_TRANSFER_SRC_OPTIMAL", VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
        .value("LAYOUT_TRANSFER_DST_OPTIMAL", VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        .value("LAYOUT_PREINITIALIZED", VK_IMAGE_LAYOUT_PREINITIALIZED)
        .export_values();

    /*************************
     * Abstraction types
     *************************/

    nb::enum_<AttachmentType>(m, "AttachmentType")
        .value("ATTACH_COLOR", AttachmentType::ATTACH_COLOR)
        .value("ATTACH_DEPTH_STENCIL", AttachmentType::ATTACH_DEPTH_STENCIL)
        .value("ATTACH_RESOLVE", AttachmentType::ATTACH_RESOLVE)
        .export_values();

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
        .def("create_framebuffer_from_targets", &VkWrappedInstance::create_framebuffer_from_targets)
        .def("create_resources", &VkWrappedInstance::create_resources)
        .def("create_sync_objects", &VkWrappedInstance::create_sync_objects)
        .def("mainloop", &VkWrappedInstance::mainloop)
        .def("get_image_buffer", &VkWrappedInstance::get_image_buffer)
        .def("create_pipeline", &VkWrappedInstance::create_pipeline)
        .def("create_attachment", &VkWrappedInstance::create_attachment)
        .def("create_render_target", &VkWrappedInstance::create_render_target)
        .def("create_render_target_from_swapchain", &VkWrappedInstance::create_render_target_from_swapchain)
        .def("find_depth_format", &VkWrappedInstance::find_depth_format)
        .def("load_mesh", &VkWrappedInstance::load_mesh);

    nb::class_<CommandBuffers> cbcl(m, "CommandBuffers");

    cbcl.def(nb::init<VkWrappedInstance*>())
        .def("alloc", &CommandBuffers::alloc);

    nb::class_<Mesh>(m, "Mesh")
        .def(nb::init<const std::vector<VERT_COMP>&, bool>())
        .def(nb::init<const Mesh&>())
        .def("load", [](Mesh& m, uint32_t v, nb::bytes& vbuf, uint32_t i, nb::bytes& ibuf) {
            m.load(v, vbuf.c_str(), vbuf.size(), i, ibuf.c_str(), ibuf.size());
        })
        .def("unload", &Mesh::unload);

    nb::class_<Lines>(m, "Lines")
        .def(nb::init<const std::vector<VERT_COMP>&>())
        .def(nb::init<const Lines&>())
        .def("load", [](Lines& line, uint32_t v, nb::bytes& vbuf) {
            line.load(v, vbuf.c_str(), vbuf.size());
        })
        .def("unload", &Lines::unload);

    nb::class_<DrawableMgr>(m, "DrawableMgr")
        .def_static("Instance", nb::overload_cast<>(&DrawableMgr::instance_ptr<>))
        .def("load_mesh", [](DrawableMgr& mgr, const std::string& name, const std::vector<VERT_COMP>& cs,
            const uint32_t v, nb::bytes& vbuf, const uint32_t i, nb::bytes& ibuf) {
                mgr.load_mesh(name, cs, v, vbuf.c_str(), vbuf.size(), i, ibuf.c_str(), ibuf.size());
        })
        .def("add_line", &DrawableMgr::add_line)
        .def("add_plane", &DrawableMgr::add_plane)
        .def("add_cube", &DrawableMgr::add_cube)
        .def("add_sphere", &DrawableMgr::add_sphere)
        .def("upload_gpu", &DrawableMgr::upload_gpu);

    nb::class_<PipelineLightStorage> plscl(m, "PipelineLightStorage");
    plscl.def_rw("pt_lights", &PipelineLightStorage::pt_lights)
        .def_rw("dir_lights", &PipelineLightStorage::dir_lights)
        .def_rw("spot_lights", &PipelineLightStorage::spot_lights);

    nb::class_<LightMgr> lmcl(m, "LightMgr");

    lmcl.def_static("Instance", nb::overload_cast<>(&LightMgr::instance_ptr<>))
        .def("add_pt_light", &LightMgr::add_pt_light)
        .def("add_dir_light", &LightMgr::add_dir_light)
        .def("add_spot_light", &LightMgr::add_spot_light)
        .def("clear_lights", &LightMgr::clear_lights)
        .def("register_pipeline", &LightMgr::register_pipeline)
        .def("unregister_pipeline", &LightMgr::unregister_pipeline)
        .def("update_uniform", &LightMgr::update_uniform);

    nb::class_<Camera> cmcl(m, "Camera");
    cmcl.def(nb::init<>())
        .def("__init__", [](Camera* self, const glm::mat4& view, const glm::mat4& proj) {
            new (self) Camera();
            self->gpu.view = view;
            self->gpu.proj = proj;
        })
        .def("look_at", &Camera::look_at)
        .def("perspective", &Camera::perspective);
}