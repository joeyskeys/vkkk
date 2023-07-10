#include <fmt/format.h>

#include "vk_ins/vkabstraction.h"
#include "vk_ins/pipeline_mgr.h"

namespace vkkk
{

Pipeline::Pipeline(VkWrappedInstance* i)
    : ins(i)
    , uniforms(i)
    , modules(i, &uniforms)
{}

Pipeline::Pipeline(Pipeline&& rhs)
    : ins(rhs.ins)
    , uniforms(std::move(rhs.uniforms))
    , modules(std::move(rhs.modules))
{}

Pipeline::~Pipeline() {}

PipelineMgr::PipelineMgr(VkWrappedInstance* i)
    : ins(i)
{}

PipelineMgr::~PipelineMgr() {}

Pipeline& PipelineMgr::register_pipeline(const std::string& name) {
    auto found = pipeline_map.find(name);
    uint32_t idx = 0;
    if (found == pipeline_map.end()) {
        idx = pipelines.size();
        pipeline_map[name] = idx;
        auto pipeline = Pipeline(ins);
        pipelines.emplace_back(std::move(pipeline));
        vk_pipelines.emplace_back(VkPipeline{});
        layouts.emplace_back(VkPipelineLayout{});
        //renderpasses.emplace_back(VkRenderPass{});
    }
    else {
        idx = found->second;
    }

    return pipelines[idx];
}

void PipelineMgr::create_pipelines(const VkRenderPass& renderpass) {
    std::vector<VkGraphicsPipelineCreateInfo> pipeline_create_infos;
    for (int i = 0; i < layouts.size(); ++i) {
        auto& pipeline = pipelines[i];
        if (!pipeline.modules.valid())
            throw std::runtime_error(fmt::format("pipeline modules are not valid for index {}", i));

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // Set is another concept which need to futher investigate
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = pipeline.modules.get_descriptor_set_layout();

        if (vkCreatePipelineLayout(ins->get_device(), &layout_info, nullptr, &layouts[i]) != VK_SUCCESS)
            throw std::runtime_error(fmt::format("failed to create pipeline layout for index {}", i));

        pipeline.modules.generate_create_infos();
        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_create_info.stageCount = pipeline.modules.get_stages_count();
        pipeline_create_info.pStages = pipeline.modules.get_stages_data();
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
        pipeline_create_info.renderPass = renderpass;
        pipeline_create_info.subpass = 0;
        pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;

        pipeline_create_infos.emplace_back(pipeline_create_info);
    }

    if (vkCreateGraphicsPipelines(ins->get_device(), VK_NULL_HANDLE, 1, pipeline_create_infos.data(), nullptr, vk_pipelines.data()) != VK_SUCCESS)
        throw std::runtime_error("pipelines creation failed");
}

}