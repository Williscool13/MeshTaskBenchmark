//
// Created by William on 2025-11-24.
//

#ifndef MESHTASKBENCHMARK_INDIRECT_TASK_MESH_COMPUTE_PIPELINE_H
#define MESHTASKBENCHMARK_INDIRECT_TASK_MESH_COMPUTE_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct IndirectTaskMeshComputePushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress instanceBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress taskIndirectParameterBuffer;
};

class IndirectTaskMeshComputePipeline
{
public:
    IndirectTaskMeshComputePipeline();

    ~IndirectTaskMeshComputePipeline();

    explicit IndirectTaskMeshComputePipeline(VulkanContext* context);

    IndirectTaskMeshComputePipeline(const IndirectTaskMeshComputePipeline&) = delete;

    IndirectTaskMeshComputePipeline& operator=(const IndirectTaskMeshComputePipeline&) = delete;

    IndirectTaskMeshComputePipeline(IndirectTaskMeshComputePipeline&& other) noexcept;

    IndirectTaskMeshComputePipeline& operator=(IndirectTaskMeshComputePipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context{};
};
} // Renderer

#endif //MESHTASKBENCHMARK_INDIRECT_TASK_MESH_COMPUTE_PIPELINE_H