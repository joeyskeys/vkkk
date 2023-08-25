#include <fmt/format.h>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/pipeline_mgr.h"

namespace vkkk
{

PipelineDeprecated::PipelineDeprecated(VkWrappedInstance* i)
    : ins(i)
    , uniforms(std::make_shared<UniformMgr>(i))
    , modules(std::make_shared<ShaderModules>(i, uniforms.get()))
    , input_info()
    , input_assembly()
    , viewport()
    , vp_state_info()
    , scissor()
    , rasterizer()
    , multisampling()
    , depth_stencil()
    , blend_attachment()
    , blend_state()
{
    input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    viewport.x = 0.f;
    viewport.y = 0.f;
    auto& extent = ins->get_swapchain_extent();
    viewport.width = extent.width;
    viewport.height = extent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    scissor.offset = { 0, 0 };
    scissor.extent = extent;

    vp_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp_state_info.viewportCount = 1;
    vp_state_info.pViewports = &viewport;
    vp_state_info.scissorCount = 1;
    vp_state_info.pScissors = &scissor;

    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depth_stencil.depthBoundsTestEnable = VK_FALSE;
    depth_stencil.stencilTestEnable = VK_FALSE;

    blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    blend_attachment.blendEnable = VK_FALSE;

    blend_state.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend_state.logicOpEnable = VK_FALSE;
    blend_state.logicOp = VK_LOGIC_OP_COPY;
    blend_state.attachmentCount = 1;
    blend_state.pAttachments = &blend_attachment;
    blend_state.blendConstants[0] = 0.f;
    blend_state.blendConstants[1] = 0.f;
    blend_state.blendConstants[2] = 0.f;
    blend_state.blendConstants[3] = 0.f;
}

PipelineDeprecated::PipelineDeprecated(const PipelineDeprecated& rhs)
    : ins(rhs.ins)
    , uniforms(rhs.uniforms)
    , modules(rhs.modules)
    , input_info(rhs.input_info)
    , input_assembly(rhs.input_assembly)
    , viewport(rhs.viewport)
    , vp_state_info(rhs.vp_state_info)
    , scissor(rhs.scissor)
    , rasterizer(rhs.rasterizer)
    , multisampling(rhs.multisampling)
    , depth_stencil(rhs.depth_stencil)
    , blend_attachment(rhs.blend_attachment)
    , blend_state(rhs.blend_state)
{
    vp_state_info.pViewports = &viewport;
    vp_state_info.pScissors = &scissor;
    blend_state.pAttachments = &blend_attachment;
}

PipelineDeprecated::PipelineDeprecated(PipelineDeprecated&& rhs)
    : ins(rhs.ins)
    , uniforms(std::move(rhs.uniforms))
    , modules(std::move(rhs.modules))
    , input_info(rhs.input_info)
    , input_assembly(rhs.input_assembly)
    , viewport(rhs.viewport)
    , vp_state_info(rhs.vp_state_info)
    , scissor(rhs.scissor)
    , rasterizer(rhs.rasterizer)
    , multisampling(rhs.multisampling)
    , depth_stencil(rhs.depth_stencil)
    , blend_attachment(rhs.blend_attachment)
    , blend_state(rhs.blend_state)
{
    // Cannot just use previous pointer values
    vp_state_info.pViewports = &viewport;
    vp_state_info.pScissors = &scissor;
    blend_state.pAttachments = &blend_attachment;
}

PipelineDeprecated::~PipelineDeprecated() {}

void PipelineDeprecated::free_gpu_resources() {
    uniforms->free_gpu_resources();
    modules->free_gpu_resources();
}

PipelineMgr::~PipelineMgr() {}

void PipelineMgr::free_gpu_resources() {
    for (auto& ppl : pipelines)
        ppl.free_gpu_resources();
}

void PipelineMgr::register_pipeline(const std::string& name) {
    auto found = pipeline_map.find(name);
    uint32_t idx = 0;
    if (found == pipeline_map.end()) {
        idx = pipelines.size();
        pipeline_map[name] = idx;
        auto pipeline = PipelineDeprecated(ins);
        pipelines.emplace_back(std::move(pipeline));
        vk_pipelines.emplace_back(VkPipeline{});
        layouts.emplace_back(VkPipelineLayout{});
        //renderpasses.emplace_back(VkRenderPass{});
    }
    // return reference or pointer to pipelines element here directly
    // will give a incorrect result, look into it later
}

void PipelineMgr::create_pipelines() {
    std::vector<VkGraphicsPipelineCreateInfo> pipeline_create_infos;
    for (int i = 0; i < layouts.size(); ++i) {
        auto& pipeline = pipelines[i];
        if (!pipeline.modules->valid())
            throw std::runtime_error(fmt::format("pipeline modules are not valid for index {}", i));

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // Set is another concept which need to futher investigate
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = pipeline.modules->get_descriptor_set_layout();

        if (vkCreatePipelineLayout(ins->get_device(), &layout_info, nullptr, &layouts[i]) != VK_SUCCESS)
            throw std::runtime_error(fmt::format("failed to create pipeline layout for index {}", i));

        pipeline.modules->generate_create_infos();
        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = pipeline.modules->get_stages_count();
        pipeline_create_info.pStages = pipeline.modules->get_stages_data();

        pipeline.input_info.vertexBindingDescriptionCount = pipeline.modules->get_binding_description_count();
        pipeline.input_info.pVertexBindingDescriptions = pipeline.modules->get_binding_descriptions();
        pipeline.input_info.vertexAttributeDescriptionCount = pipeline.modules->get_attr_description_count();
        pipeline.input_info.pVertexAttributeDescriptions = pipeline.modules->get_attr_descriptions();
        pipeline_create_info.pVertexInputState = &pipeline.input_info;

        pipeline_create_info.pInputAssemblyState = &pipeline.input_assembly;
        pipeline_create_info.pViewportState = &pipeline.vp_state_info;
        pipeline_create_info.pRasterizationState = &pipeline.rasterizer;
        pipeline_create_info.pMultisampleState = &pipeline.multisampling;
        pipeline_create_info.pDepthStencilState = &pipeline.depth_stencil;
        pipeline_create_info.pColorBlendState = &pipeline.blend_state;
        pipeline_create_info.layout = layouts[i];
        // Renderpass is bound to framebuffer or other buffers
        // When we're trying to render into another buffer, this code will
        // cause problem.
        pipeline_create_info.renderPass = ins->get_renderpass();
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

        pipeline_create_infos.emplace_back(pipeline_create_info);

        /*
        vkCreateGraphicsPipelines(ins->get_device(), VK_NULL_HANDLE, 1,
            &pipeline_create_infos[i], nullptr, &vk_pipelines[i]);
        */
    }

    if (vkCreateGraphicsPipelines(ins->get_device(), VK_NULL_HANDLE, pipeline_create_infos.size(),
        pipeline_create_infos.data(), nullptr, vk_pipelines.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("pipelines creation failed");
    }
}

}