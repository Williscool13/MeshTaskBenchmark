//
// Created by William on 2025-11-23.
//

#ifndef MESHTASKBENCHMARK_TRADITIONAL_INDIRECT_RENDER_PIPELINE_H
#define MESHTASKBENCHMARK_TRADITIONAL_INDIRECT_RENDER_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct TraditionalIndirectRenderPushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress materialBuffer;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;
};

class TraditionalIndirectRenderPipeline
{
public:
    TraditionalIndirectRenderPipeline();

    ~TraditionalIndirectRenderPipeline();

    explicit TraditionalIndirectRenderPipeline(VulkanContext* context);

    TraditionalIndirectRenderPipeline(const TraditionalIndirectRenderPipeline&) = delete;

    TraditionalIndirectRenderPipeline& operator=(const TraditionalIndirectRenderPipeline&) = delete;

    TraditionalIndirectRenderPipeline(TraditionalIndirectRenderPipeline&& other) noexcept;

    TraditionalIndirectRenderPipeline& operator=(TraditionalIndirectRenderPipeline&& other) noexcept;


public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //MESHTASKBENCHMARK_TRADITIONAL_INDIRECT_RENDER_PIPELINE_H