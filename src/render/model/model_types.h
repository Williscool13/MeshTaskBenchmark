//
// Created by William on 2025-11-23.
//

#ifndef MESHTASKBENCHMARK_MODEL_TYPES_H
#define MESHTASKBENCHMARK_MODEL_TYPES_H

#include <filesystem>
#include <string>
#include <glm/glm.hpp>

#include "offsetAllocator.hpp"

namespace Renderer
{
enum class MaterialType
{
    OPAQUE_ = 0,
    TRANSPARENT_ = 1,
    MASK_ = 2,
};

struct Vertex
{
    glm::vec3 position{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec4 tangent{1.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 color{1.0f};
    glm::vec2 uv{0, 0};
};

struct MaterialProperties
{
    // Base PBR properties
    glm::vec4 colorFactor{1.0f};
    glm::vec4 metalRoughFactors{0.0f, 1.0f, 0.0f, 0.0f}; // x: metallic, y: roughness, z: pad, w: pad

    // Texture indices
    glm::ivec4 textureImageIndices{-1}; // x: color, y: metallic-rough, z: normal, w: emissive
    glm::ivec4 textureSamplerIndices{-1}; // x: color, y: metallic-rough, z: normal, w: emissive
    glm::ivec4 textureImageIndices2{-1}; // x: occlusion, y: packed NRM, z: pad, w: pad
    glm::ivec4 textureSamplerIndices2{-1}; // x: occlusion, y: packed NRM, z: pad, w: pad

    // UV transform properties (scale.xy, offset.xy for each texture type)
    glm::vec4 colorUvTransform{1.0f, 1.0f, 0.0f, 0.0f}; // xy: scale, zw: offset
    glm::vec4 metalRoughUvTransform{1.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 normalUvTransform{1.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 emissiveUvTransform{1.0f, 1.0f, 0.0f, 0.0f};
    glm::vec4 occlusionUvTransform{1.0f, 1.0f, 0.0f, 0.0f};

    // Additional material properties
    glm::vec4 emissiveFactor{0.0f, 0.0f, 0.0f, 1.0f}; // xyz: emissive color, w: emissive strength
    glm::vec4 alphaProperties{0.5f, 0.0f, 0.0f, 0.0f}; // x: alpha cutoff, y: alpha mode, z: double sided, w: unlit
    glm::vec4 physicalProperties{1.5f, 0.0f, 1.0f, 0.0f}; // x: IOR, y: dispersion, z: normal scale, w: occlusion strength
};


struct TraditionalPrimitive
{
    uint32_t firstIndex{0};
    uint32_t indexCount{0};
    int32_t vertexOffset{0};
    uint32_t materialIndex{0};
    // {3} center, {1} radius
    glm::vec4 boundingSphere{};
};

struct MeshletPrimitive
{
    uint32_t meshletOffset{0};
    uint32_t meshletCount{0};
    uint32_t materialIndex{0};
    uint32_t padding{0};
    // {3} center, {1} radius
    glm::vec4 boundingSphere{};
};

struct Meshlet
{
    glm::vec4 meshletBoundingSphere;

    glm::vec3 coneApex;
    float coneCutoff;

    glm::vec3 coneAxis;
    uint32_t vertexOffset;

    uint32_t meshletVerticesOffset;
    uint32_t meshletTriangleOffset;
    uint32_t meshletVerticesCount;
    uint32_t meshletTriangleCount;
};


struct Instance
{
    uint32_t primitiveIndex{INT32_MAX};
    uint32_t modelIndex{INT32_MAX};
    uint32_t jointMatrixOffset{};
    uint32_t bIsAllocated{false};
};

struct Model
{
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 prevModelMatrix{1.0f};
    glm::vec4 flags{1.0f}; // x: visible, y: shadow-caster, zw: reserved
};


struct TraditionalIndirectDrawParameters
{
    uint32_t indexCount;
    uint32_t instanceCount;
    uint32_t firstIndex;
    int32_t vertexOffset;
    uint32_t firstInstance;
};


struct TaskIndirectDrawParameters
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
    uint32_t padding;

    // instance/primitive properties
    uint32_t modelIndex; // public uint32_t jointMatrixOffset; - they are mutually exclusive, but for simplcity maybe just have both?
    uint32_t materialIndex;
    uint32_t meshletOffset;
    uint32_t meshletCount;
};

struct MeshInformation
{
    std::string name;
    std::vector<uint32_t> primitiveIndices;
};

struct ModelData
{
    std::string name{};
    std::filesystem::path path{};

    uint32_t indexCount{};
    uint32_t indexOffset{};
    uint32_t vertexOffset{};

    uint32_t meshletCount{};

    std::vector<MeshInformation> meshes{};

    OffsetAllocator::Allocation vertexAllocation{};

    // Traditional
    OffsetAllocator::Allocation indexAllocation{};
    OffsetAllocator::Allocation traditionalPrimitiveAllocation{};

    // Meshlet
    OffsetAllocator::Allocation meshletVerticesAllocation{};
    OffsetAllocator::Allocation meshletTrianglesAllocation{};
    OffsetAllocator::Allocation meshletAllocation{};

    OffsetAllocator::Allocation materialAllocation{};
    OffsetAllocator::Allocation meshletPrimitiveAllocation{};


    ModelData() = default;

    ModelData(const ModelData&) = delete;

    ModelData& operator=(const ModelData&) = delete;

    ModelData(ModelData&&) noexcept = default;

    ModelData& operator=(ModelData&&) noexcept = default;
};
} // Renderer

#endif //MESHTASKBENCHMARK_MODEL_TYPES_H
