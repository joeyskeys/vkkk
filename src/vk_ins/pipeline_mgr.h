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

    std::vector<Pipeline>                   pipeline_infos;
    std::vector<VkPipeline>                 pipelines;
    std::vector<VkPipelineLayout>           layouts;
    std::vector<VkRenderPass>               renderpasses;
    std::map<std::string, uint32_t>         pipeline_map;

    PipelineMgr(VkWrappedInstance*);
    virtual ~PipelineMgr();

    Pipeline&   register_pipeline(const std::string&);
    bool        create_pipelines();
};

}