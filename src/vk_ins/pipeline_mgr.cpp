#include <boost/container_hash/hash.hpp>
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

Pipeline::~Pipeline() {}

PipelineMgr::PipelineMgr(VkWrappedInstance* i)
    : ins(i)
{}

PipelineMgr::~PipelineMgr() {
    for (auto& [hash, layout] : layouts)
        vkDestroyPipelineLayout(ins->get_device(), layout, nullptr);
}

Pipeline& register_pipeline(const std::string& name) {
    auto found = pipeline_map.find(name);
    uint32_t idx = 0;
    if (found == pipeline_map.end()) {
        idx = pipelines.size();
        pipeline_map[name] = idx;
        pipelines.emplace_back(Pipeline(ins));
        vk_pipelines.emplace_back(VkPipeline{});
        layouts.emplace_back(VkPipelineLayout{});
        //renderpasses.emplace_back(VkRenderPass{});
    }
    else {
        idx = *found;
    }

    return pipelines[idx];
}

void PipelineMgr::create_pipelines(const VkRenderPass& renderpass) {
    std::vector<VkGraphicsPipelineCreateInfo> pipeline_create_infos;
    for (int i = 0; i < layouts.size(); ++i) {
        auto& pipeline_info = pipeline_infos[i];
        if (!pipeline_info.modules.valid())
            throw std::runtime_error(fmt::format("pipeline {} modules are not valid", name));

        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        // Set is another concept which need to futher investigate
        layout_info.setLayoutCount = 1;
        layout_info.pSetLayouts = pipeline_info.modules.get_descriptor_set_layout();

        if (vkCreatePipelineLayout(ins->get_device(), &layout_info, nullptr, &pipeline.layout) != VK_SUCCESS)
            throw std::runtime(fmt::format("failed to create pipeline layout for {}", name));

        pipeline_info.modules.generate_create_infos();
        VkGraphicsPipelineCreateInfo pipeline_create_info{};
        pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipeline_info.stageCount = pipeline_info.modules.get_stages_count();
        pipeline_info.pStages = pipeline_info.modules.get_stages_data();
        pipeline_info.pVertexInputState = &pipeline_info.input_info;
        pipeline_info.pInputAssemblyState = &pipeline_info.input_assembly;
        pipeline_info.pViewportState = &pipeline_info.viewport;
        pipeline_info.pRasterizationState = &pipeline_info.rasterizer;
        pipeline_info.pMultisampleState = &pipeline_info.multisampling;
        pipeline_info.pDepthStencilState = &pipeline_info.depth_stencil;
        pipeline_info.pColorBlendState = &pipeline_info.blend_state;
        pipeline_info.layout = layouts[i];
        // Renderpass is bound to framebuffer or other buffers
        // When we're trying to render into another buffer, this code will
        // cause problem.
        pipeline_info.renderPass = renderpass;
        pipeline_info.subpass = 0;
        pipeline_info.basePipelineHandle = VK_NULL_HANDLE;

        pipeline_infos.emplace_back(pipeline_info);
    }

    if (vkCreateGraphicsPipelines(ins->get_device(), VK_NULL_HANDLE, 1, pipeline_create_infos.data(), nullptr, pipelines.data()) != VK_SUCCESS)
        throw std::runtime_error("pipelines creation failed");
}

}