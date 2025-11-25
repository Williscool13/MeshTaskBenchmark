//
// Created by William on 2025-11-22.
//

#ifndef MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H
#define MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H
#include <memory>
#include <vector>
#include <SDL3/SDL.h>

#include "offsetAllocator.hpp"
#include "utils.h"
#include "core/data-structures/handle_allocator.h"
#include "game/camera/free_camera.h"
#include "render/render_constants.h"
#include "render/vk_resources.h"
#include "render/vk_types.h"
#include "render/model/model_types.h"
#include "render/pipelines/indirect_task_mesh_compute_pipeline.h"
#include "render/pipelines/indirect_task_mesh_render_pipeline.h"
#include "render/pipelines/task_mesh_pipeline.h"
#include "render/pipelines/traditional_indirect_compute_pipeline.h"
#include "render/pipelines/traditional_indirect_render_pipeline.h"
#include "render/pipelines/traditional_pipeline.h"

namespace Renderer
{
struct FrameSynchronization;
struct VulkanContext;
struct Swapchain;
struct RenderTargets;
}

static inline constexpr int32_t DEFAULT_WINDOW_WIDTH = 1700;
static inline constexpr int32_t DEFAULT_WINDOW_HEIGHT = 900;

class MeshTaskBenchmark
{
public:
    MeshTaskBenchmark();
    ~MeshTaskBenchmark();

    void Initialize();

    void Run();

    void Render(uint32_t currentFrameInFlight, Renderer::FrameSynchronization& frameSync);

    void Cleanup();

    void CreateBuffers();

    void InstanceGeneration();

    Renderer::ModelData LoadModel(const std::filesystem::path& path);

    glm::vec4 GenerateBoundingSphere(const std::vector<Renderer::Vertex>& vertices);

    void Traditional(uint32_t currentFrameInFlight, std::array<uint32_t, 2> extents, VkCommandBuffer cmd);

    void IndirectTraditional(uint32_t currentFrameInFlight, std::array<uint32_t, 2> extents, VkCommandBuffer cmd);

    void Meshlet(uint32_t currentFrameInFlight, std::array<uint32_t, 2> extents, VkCommandBuffer cmd);

    void IndirectMeshlet(uint32_t currentFrameInFlight, std::array<uint32_t, 2> extents, VkCommandBuffer cmd);

private:
    enum class BenchmarkType
    {
        Traditional,
        IndirectTraditional,
        Meshlet,
        IndirectMeshlet,
    };
    BenchmarkType benchmarkType = BenchmarkType::Traditional;

    SDL_Window* window{nullptr};
    std::unique_ptr<Renderer::VulkanContext> context{};
    std::unique_ptr<Renderer::Swapchain> swapchain;
    std::unique_ptr<Renderer::RenderTargets> renderTargets;
    std::vector<Renderer::FrameSynchronization> frameSynchronization;

    bool bShouldExit{false};
    uint32_t frameNumber{0};

    Game::FreeCamera freeCamera{{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}};
    Renderer::SceneData sceneData{};
    std::vector<Renderer::AllocatedBuffer> sceneDataBuffers;

    Renderer::TraditionalPipeline traditionalPipeline{};
    Renderer::TraditionalIndirectComputePipeline indirectTraditionalCompute{};
    Renderer::TraditionalIndirectRenderPipeline indirectTraditionalGraphics{};
    Renderer::TaskMeshPipeline taskMeshPipeline{};
    Renderer::IndirectTaskMeshComputePipeline indirectTaskMeshCompute{};
    Renderer::IndirectTaskMeshRenderPipeline indirectTaskMeshGraphics{};

    Renderer::ModelData bunnyModel{};

private:
    Renderer::AllocatedBuffer megaVertexBuffer;
    OffsetAllocator::Allocator vertexBufferAllocator{sizeof(Renderer::Vertex) * Renderer::MEGA_VERTEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer materialBuffer;
    OffsetAllocator::Allocator materialBufferAllocator{sizeof(Renderer::MaterialProperties) * Renderer::MEGA_MATERIAL_BUFFER_COUNT};

    // Traditional
    Renderer::AllocatedBuffer megaIndexBuffer;
    OffsetAllocator::Allocator indexBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer traditionalPrimitiveBuffer;
    OffsetAllocator::Allocator traditionalPrimitiveBufferAllocator{sizeof(Renderer::MeshletPrimitive) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT};

    // Traditional indirect
    Renderer::AllocatedBuffer traditionalIndirectBuffer;

    // Meshlet
    Renderer::AllocatedBuffer megaMeshletVerticesBuffer;
    OffsetAllocator::Allocator meshletVerticesBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer megaMeshletTrianglesBuffer;
    OffsetAllocator::Allocator meshletTrianglesBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer megaMeshletBuffer;
    OffsetAllocator::Allocator meshletBufferAllocator{sizeof(uint32_t) * Renderer::MEGA_INDEX_BUFFER_COUNT};
    Renderer::AllocatedBuffer meshletPrimitiveBuffer;
    OffsetAllocator::Allocator meshletPrimitiveBufferAllocator{sizeof(Renderer::MeshletPrimitive) * Renderer::MEGA_PRIMITIVE_BUFFER_COUNT};

    HandleAllocator<Renderer::Model, Renderer::BINDLESS_MODEL_MATRIX_COUNT> modelMatrixAllocator;
    Renderer::AllocatedBuffer modelBuffer;
    HandleAllocator<Renderer::Instance, Renderer::BINDLESS_INSTANCE_COUNT> instanceEntryAllocator;
    Renderer::AllocatedBuffer instanceBuffer;

    // Meshlet indirect
    Renderer::AllocatedBuffer meshletIndirectBuffer;

private:
    float benchmarkTimer = 0.0f;
    int benchmarkFrameCount = 0;
    float averageFPS = 0.0f;
    float fpsDisplayTimer = 0.0f;
    BenchmarkType lastBenchmarkType = BenchmarkType::Traditional;
};


#endif //MESHTASKBENCHMARK_MESH_TASK_BENCHMARK_H