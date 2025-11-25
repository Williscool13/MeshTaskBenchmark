//
// Created by William on 2025-11-24.
//

#include "indirect_task_mesh_compute_pipeline.h"

#include <fmt/format.h>
#include <filesystem>

#include "render/vk_helpers.h"
#include "render/vk_types.h"

namespace Renderer
{
IndirectTaskMeshComputePipeline::IndirectTaskMeshComputePipeline() = default;

IndirectTaskMeshComputePipeline::~IndirectTaskMeshComputePipeline() = default;

IndirectTaskMeshComputePipeline::IndirectTaskMeshComputePipeline(VulkanContext* context)
    : context(context)
{
    VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo{};
    computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computePipelineLayoutCreateInfo.pNext = nullptr;
    computePipelineLayoutCreateInfo.pSetLayouts = nullptr;
    computePipelineLayoutCreateInfo.setLayoutCount = 0;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(IndirectTaskMeshComputePushConstant);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
    computePipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    pipelineLayout = VkResources::CreatePipelineLayout(context, computePipelineLayoutCreateInfo);

    VkShaderModule computeShader;
    std::filesystem::path shaderPath = {"shaders/indirectMeshTask_compute.spv"};
    if (!VkHelpers::LoadShaderModule(shaderPath.string().c_str(), context->device, &computeShader)) {
        fmt::println("Failed to load {}", shaderPath.string());
        exit(1);
    }

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = VkHelpers::PipelineShaderStageCreateInfo(computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
    VkComputePipelineCreateInfo computePipelineCreateInfo = VkHelpers::ComputePipelineCreateInfo(pipelineLayout.handle, pipelineShaderStageCreateInfo);
    pipeline = VkResources::CreateComputePipeline(context, computePipelineCreateInfo);

    // Cleanup
    vkDestroyShaderModule(context->device, computeShader, nullptr);
}

IndirectTaskMeshComputePipeline::IndirectTaskMeshComputePipeline(IndirectTaskMeshComputePipeline&& other) noexcept
{
    pipelineLayout = std::move(other.pipelineLayout);
    pipeline = std::move(other.pipeline);
    context = other.context;
    other.context = nullptr;
}

IndirectTaskMeshComputePipeline& IndirectTaskMeshComputePipeline::operator=(IndirectTaskMeshComputePipeline&& other) noexcept
{
    if (this != &other) {
        pipelineLayout = std::move(other.pipelineLayout);
        pipeline = std::move(other.pipeline);
        context = other.context;
        other.context = nullptr;
    }
    return *this;
}
} // Renderer
