//
// Created by William on 2025-11-23.
//

#ifndef MESHTASKBENCHMARK_TRADITIONAL_PIPELINE_H
#define MESHTASKBENCHMARK_TRADITIONAL_PIPELINE_H
#include <complex.h>

#include "render/vk_resources.h"

namespace Renderer
{

struct TraditionalPipelinePushConstant {
    VkDeviceAddress sceneData;
    VkDeviceAddress materialBuffer;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;
};

class TraditionalPipeline
{
public:
    TraditionalPipeline();

    ~TraditionalPipeline();

    explicit TraditionalPipeline(VulkanContext* context);

    TraditionalPipeline(const TraditionalPipeline&) = delete;

    TraditionalPipeline& operator=(const TraditionalPipeline&) = delete;

    TraditionalPipeline(TraditionalPipeline&& other) noexcept;

    TraditionalPipeline& operator=(TraditionalPipeline&& other) noexcept;


public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context;
};
} // Renderer

#endif //MESHTASKBENCHMARK_TRADITIONAL_PIPELINE_H