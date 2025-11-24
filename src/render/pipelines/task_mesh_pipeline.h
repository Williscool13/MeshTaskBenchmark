//
// Created by William on 2025-11-24.
//

#ifndef MESHTASKBENCHMARK_TASK_MESH_PIPELINE_H
#define MESHTASKBENCHMARK_TASK_MESH_PIPELINE_H

#include "render/vk_resources.h"

namespace Renderer
{
struct VulkanContext;

struct TaskMeshPushConstant
{
    VkDeviceAddress sceneData;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress primitiveBuffer;
    VkDeviceAddress meshletVerticesBuffer;
    VkDeviceAddress meshletTrianglesBuffer;
    VkDeviceAddress meshletBuffer;
    VkDeviceAddress materialBuffer;
    VkDeviceAddress modelBuffer;
    VkDeviceAddress instanceBuffer;
};


class TaskMeshPipeline
{
public:
    TaskMeshPipeline();

    ~TaskMeshPipeline();

    TaskMeshPipeline(VulkanContext* context);

    TaskMeshPipeline(const TaskMeshPipeline&) = delete;

    TaskMeshPipeline& operator=(const TaskMeshPipeline&) = delete;

    TaskMeshPipeline(TaskMeshPipeline&& other) noexcept;

    TaskMeshPipeline& operator=(TaskMeshPipeline&& other) noexcept;

public:
    PipelineLayout pipelineLayout;
    Pipeline pipeline;

private:
    VulkanContext* context{nullptr};
};
}

#endif //MESHTASKBENCHMARK_TASK_MESH_PIPELINE_H