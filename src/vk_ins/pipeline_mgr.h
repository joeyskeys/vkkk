#pragma once

#include <map>

#include <vulkan/vulkan.h>

#include "utils/singleton.h"
#include "vk_ins/cmd_buf.h"
#include "vk_ins/shader_mgr.h"

namespace vkkk
{

class VkWrappedInstance;

class PipelineDeprecated {
public:
    VkWrappedInstance*                      ins;
    std::shared_ptr<UniformMgr>             uniforms;
    std::shared_ptr<ShaderModulesDeprecated>          modules;
    VkPipelineVertexInputStateCreateInfo    input_info;
    VkPipelineInputAssemblyStateCreateInfo  input_assembly;
    VkViewport                              viewport;
    VkPipelineViewportStateCreateInfo       vp_state_info;
    VkRect2D                                scissor;
    VkPipelineRasterizationStateCreateInfo  rasterizer;
    VkPipelineMultisampleStateCreateInfo    multisampling;
    VkPipelineDepthStencilStateCreateInfo   depth_stencil;
    VkPipelineColorBlendAttachmentState     blend_attachment;
    VkPipelineColorBlendStateCreateInfo     blend_state;
    
    PipelineDeprecated(VkWrappedInstance* i);
    PipelineDeprecated(const PipelineDeprecated& rhs);
    PipelineDeprecated(PipelineDeprecated&& rhs);
    virtual ~PipelineDeprecated();

    void free_gpu_resources();
};

class PipelineMgr : public Singleton<PipelineMgr> {
private:
    // For singleton pattern
    PipelineMgr(VkWrappedInstance* i) : ins(i) {}
    friend class Singleton<PipelineMgr>;

    PipelineMgr(const PipelineMgr& rhs) = delete;
    PipelineMgr& operator= (const PipelineMgr& rhs) = delete;
    
public:
    VkWrappedInstance*                      ins;

    std::vector<PipelineDeprecated>         pipelines;
    std::vector<VkPipeline>                 vk_pipelines;
    std::vector<VkPipelineLayout>           layouts;
    std::map<std::string, uint32_t>         pipeline_map;

    virtual ~PipelineMgr();

    void            free_gpu_resources();

    void            register_pipeline(const std::string&);
    void            create_pipelines();

    inline void     create_descriptor_layouts() {
        for (auto& pipeline : pipelines)
            pipeline.modules->create_descriptor_layouts();
    }

    inline void     create_input_descriptions(const std::vector<VERT_COMP>& comps) {
        for (auto& pipeline : pipelines)
            pipeline.modules->create_input_descriptions(comps);
    }
    
    inline void     create_descriptor_pools() {
        // Here's another possible design:
        // Create a single pool for all modules and you will only to create
        // the pool once
        for (auto& pipeline : pipelines)
            pipeline.modules->create_descriptor_pool();
    }

    inline void     create_descriptor_sets() {
        for (auto& pipeline : pipelines)
            pipeline.modules->create_descriptor_set();
    }

    inline PipelineDeprecated& get_pipeline(const uint32_t idx) {
        return pipelines.at(idx);
    }

    inline PipelineDeprecated& get_pipeline(const std::string& name) {
        auto idx = pipeline_map[name];
        return pipelines.at(idx);
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

    inline const std::pair<VkPipeline, VkPipelineLayout> get_vkpipeline_and_layout(const std::string& name) const {
        auto found = pipeline_map.find(name);
        if (found == pipeline_map.end())
            return std::make_pair(nullptr, nullptr);
        else
            return std::make_pair(vk_pipelines[found->second], layouts[found->second]);
    }

    inline void bind(const std::string& name, CommandBuffers& bufs, uint32_t idx) {
        auto found = pipeline_map.find(name);
        if (found == pipeline_map.end())
            return;
        else
            vkCmdBindPipeline(bufs[idx], VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipelines[found->second]);
    }
};

}