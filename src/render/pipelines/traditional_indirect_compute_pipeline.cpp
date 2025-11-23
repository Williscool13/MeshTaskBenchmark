//
// Created by William on 2025-11-23.
//

#include "traditional_indirect_compute_pipeline.h"

#include <fmt/format.h>
#include <filesystem>

#include "render/vk_helpers.h"

namespace Renderer
{
TraditionalIndirectComputePipeline::TraditionalIndirectComputePipeline() = default;

TraditionalIndirectComputePipeline::~TraditionalIndirectComputePipeline() = default;

TraditionalIndirectComputePipeline::TraditionalIndirectComputePipeline(VulkanContext* context)
    : context(context)
{
    VkPipelineLayoutCreateInfo computePipelineLayoutCreateInfo{};
    computePipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computePipelineLayoutCreateInfo.pNext = nullptr;
    computePipelineLayoutCreateInfo.pSetLayouts = nullptr;
    computePipelineLayoutCreateInfo.setLayoutCount = 0;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(TraditionalIndirectComputePushConstant);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    computePipelineLayoutCreateInfo.pPushConstantRanges = &pushConstant;
    computePipelineLayoutCreateInfo.pushConstantRangeCount = 1;

    drawCullPipelineLayout = VkResources::CreatePipelineLayout(context, computePipelineLayoutCreateInfo);

    VkShaderModule computeShader;
    std::filesystem::path shaderPath = {"shaders/indirectTraditional_compute.spv"};
    if (!VkHelpers::LoadShaderModule(shaderPath.string().c_str(), context->device, &computeShader)) {
        fmt::println("Failed to load {}", shaderPath.string());
        exit(1);
    }

    VkPipelineShaderStageCreateInfo pipelineShaderStageCreateInfo = VkHelpers::PipelineShaderStageCreateInfo(computeShader, VK_SHADER_STAGE_COMPUTE_BIT);
    VkComputePipelineCreateInfo computePipelineCreateInfo = VkHelpers::ComputePipelineCreateInfo(drawCullPipelineLayout.handle, pipelineShaderStageCreateInfo);
    drawCullPipeline = VkResources::CreateComputePipeline(context, computePipelineCreateInfo);

    // Cleanup
    vkDestroyShaderModule(context->device, computeShader, nullptr);
}

TraditionalIndirectComputePipeline::TraditionalIndirectComputePipeline(TraditionalIndirectComputePipeline&& other) noexcept
{
    drawCullPipelineLayout = std::move(other.drawCullPipelineLayout);
    drawCullPipeline = std::move(other.drawCullPipeline);
    context = other.context;
    other.context = nullptr;
}

TraditionalIndirectComputePipeline& TraditionalIndirectComputePipeline::operator=(TraditionalIndirectComputePipeline&& other) noexcept
{
    if (this != &other) {
        drawCullPipelineLayout = std::move(other.drawCullPipelineLayout);
        drawCullPipeline = std::move(other.drawCullPipeline);
        context = other.context;
        other.context = nullptr;
    }
    return *this;
}
} // Renderer