//
// Created by William on 2025-11-23.
//

#ifndef MESHTASKBENCHMARK_TRADITIONAL_INDIRECT_COMPUTE_PIPELINE_H
#define MESHTASKBENCHMARK_TRADITIONAL_INDIRECT_COMPUTE_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct TraditionalIndirectComputePushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;
    VkDeviceAddress indirectBuffer;
};

class TraditionalIndirectComputePipeline
{
public:
    TraditionalIndirectComputePipeline();

    ~TraditionalIndirectComputePipeline();

    explicit TraditionalIndirectComputePipeline(VulkanContext* context);

    TraditionalIndirectComputePipeline(const TraditionalIndirectComputePipeline&) = delete;

    TraditionalIndirectComputePipeline& operator=(const TraditionalIndirectComputePipeline&) = delete;

    TraditionalIndirectComputePipeline(TraditionalIndirectComputePipeline&& other) noexcept;

    TraditionalIndirectComputePipeline& operator=(TraditionalIndirectComputePipeline&& other) noexcept;

public:
    PipelineLayout drawCullPipelineLayout;
    Pipeline drawCullPipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //MESHTASKBENCHMARK_TRADITIONAL_INDIRECT_COMPUTE_PIPELINE_H