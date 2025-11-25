//
// Created by William on 2025-11-24.
//

#ifndef MESHTASKBENCHMARK_INDIRECT_TASK_MESH_RENDER_PIPELINE_H
#define MESHTASKBENCHMARK_INDIRECT_TASK_MESH_RENDER_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct VulkanContext;

struct IndirectTaskMeshRenderPushConstant
{
    VkDeviceAddress sceneData;

    // Statics
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress meshletVerticesBuffer;
    VkDeviceAddress meshletTrianglesBuffer;
    VkDeviceAddress meshletBuffer;

    VkDeviceAddress meshIndirectParameterBuffer;

    // Dynamics
    VkDeviceAddress materialBuffer; // well, this ones not yet dynamic
    VkDeviceAddress modelBuffer;
};


class IndirectTaskMeshRenderPipeline
{
public:
    IndirectTaskMeshRenderPipeline();

    ~IndirectTaskMeshRenderPipeline();

    IndirectTaskMeshRenderPipeline(VulkanContext* context);

    IndirectTaskMeshRenderPipeline(const IndirectTaskMeshRenderPipeline&) = delete;

    IndirectTaskMeshRenderPipeline& operator=(const IndirectTaskMeshRenderPipeline&) = delete;

    IndirectTaskMeshRenderPipeline(IndirectTaskMeshRenderPipeline&& other) noexcept;

    IndirectTaskMeshRenderPipeline& operator=(IndirectTaskMeshRenderPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context{nullptr};
};
} // Renderer

#endif //MESHTASKBENCHMARK_INDIRECT_TASK_MESH_RENDER_PIPELINE_H