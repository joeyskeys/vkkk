#pragma once

#include <map>

#include <vulkan/vulkan.h>

#include "vk_ins/shader_mgr.h"

namespace vkkk
{

class VkWrappedInstance;

class Pipeline {
public:
    VkWrappedInstance*                      ins;
    UniformMgr                              uniforms;
    ShaderModules                           modules;
    VkPipelineVertexInputStateCreateInfo    input_info;
    VkPipelineInputAssemblyStateCreateInfo  input_assembly;
    VkViewport                              viewport;
    VkRect2D                                scissor;
    VkPipelineRasterizationStateCreateInfo  rasterizer;
    VkPipelineMultisampleStateCreateInfo    multisampling;
    VkPipelineDepthStencilStateCreateInfo   dpeth_stencil;
    VkPipelineColorBlendAttachmentState     blend_attachement;
    VkPipelineColorBlendStateCreateInfo     blend_state;
    
    Pipeline(VkWrappedInstance* i);
    virtual ~Pipeline();
};

class PipelineMgr {
public:
    VkWrappedInstance*                      ins;

    std::vector<Pipeline>                   pipelines;
    std::vector<VkPipeline>                 vk_pipelines;
    std::vector<VkPipelineLayout>           layouts;
    //std::vector<VkRenderPass>               renderpasses;
    std::map<std::string, uint32_t>         pipeline_map;

    PipelineMgr(VkWrappedInstance*);
    virtual ~PipelineMgr();

    Pipeline&       register_pipeline(const std::string&);
    bool            create_pipelines(const VkRenderPass& renderpass);

    inline void     create_descriptor_layouts() {
        for (auto& pipeline : pipelines)
            pipeline.modules.create_descriptor_layouts();
    }

    inline void     create_input_descriptions() {
        for (auto& pipeline : pipelines)
            pipeline.modules.create_input_descriptions();
    }
    
    inline void     create_descriptor_pools() {
        // Here's another possible design:
        // Create a single pool for all modules and you will only to create
        // the pool once
        for (auto& pipeline : pipelines)
            pipeline.modules.create_descriptor_pool();
    }

    inline void     create_descriptor_sets() {
        for (auto& pipeline : pipelines)
            pipeline.modules.create_descriptor_set();
    }

    inline const VkPipeline get_vkpipeline(const std::string& name) const {
        auto found = pipeline_map.find(name);
        if (found == pipeline_map.end())
            return nullptr;
        else
            return vk_pipelines[found->second];
    }

    inline const VkPipelineLayout get_vkpipeline_layout(const std::string& name) const {
        auto found = pipeline_map.find(name);
        if (found == pipeline_map.end())
            return nullptr;
        else
            return layouts[found->second];
    }
};

}