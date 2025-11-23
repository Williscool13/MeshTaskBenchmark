import common;

// ==================== Compute Shader (Instance Culling) ====================
struct PushConstsCompute {
    SceneData* sceneData;

    TraditionalPrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;

    IndexedIndirectBuffer* indirectBuffer;
};

bool frustumCullSphere(Frustum frustum, float4 sphere) {
    // sphere.xyz = center, sphere.w = radius
    for (int i = 0; i < 6; i++) {
        float4 plane = frustum.planes[i];
        float dist = dot(plane.xyz, sphere.xyz) + plane.w;
        if (dist < -sphere.w) {
            return false;
        }
    }
    return true;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void computeMain(uniform PushConstsCompute pushConstants, uint3 threadId : SV_DispatchThreadID)
{
    uint32_t instanceId = threadId.x;
    Instance instance = pushConstants.instanceBuffer->instances[instanceId];
    if (instance.bIsAllocated == 0) {
        return;
    }
    Model model = pushConstants.modelBuffer->models[instance.modelIndex];
    TraditionalPrimitive prim = pushConstants.primitiveBuffer->primitives[instance.primitiveIndex];

    // Transform bounding sphere to world space
    float4 boundingSphere = prim.boundingSphere;
    float3 worldCenter = mul(model.modelMatrix, float4(boundingSphere.xyz, 1.0)).xyz;
    float4 worldBounds = float4(worldCenter, boundingSphere.w);

    // Frustum culling
    if (!frustumCullSphere(pushConstants.sceneData->frustum, worldBounds)) {
        return;  // Instance culled
    }

    // Write draw command for visible instance
    VkDrawIndexedIndirectCommand cmd;
    cmd.indexCount = prim.indexCount;
    cmd.firstIndex = prim.firstIndex;
    cmd.vertexOffset = prim.vertexOffset;
    cmd.instanceCount = 1;
    cmd.firstInstance = instanceId;

    uint32_t outputIndex = pushConstants.indirectBuffer->drawCount.add(1);
    pushConstants.indirectBuffer->commands[outputIndex] = cmd;
}

// ==================== Vertex Shader ====================
struct VertexOutput {
    float4 position : SV_Position;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    nointerpolation uint materialIndex : MATERIAL_INDEX;
};

struct PushConsts {
    SceneData* sceneData;
    MaterialData* materialBuffer;
    TraditionalPrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;
};

[shader("vertex")]
VertexOutput vertexMain(
    Vertex vertex,
    uniform PushConsts pushConstants,
    uint startInstanceId : SV_StartInstanceLocation)
{
    Instance instance = pushConstants.instanceBuffer->instances[startInstanceId];
    Model model = pushConstants.modelBuffer->models[instance.modelIndex];
    TraditionalPrimitive primitive = pushConstants.primitiveBuffer->primitives[instance.primitiveIndex];

    float4 worldPos = mul(model.modelMatrix, float4(vertex.position, 1.0));
    float3 worldNormal = normalize(mul((float3x3)model.modelMatrix, vertex.normal));
    float3 worldTangent = normalize(mul((float3x3)model.modelMatrix, vertex.tangent.xyz));

    VertexOutput output;
    output.position = mul(pushConstants.sceneData->viewProj, worldPos);
    output.worldPos = worldPos.xyz;
    output.normal = worldNormal;
    output.tangent = float4(worldTangent, vertex.tangent.w);
    output.uv = vertex.uv;
    output.color = vertex.color;
    output.materialIndex = primitive.materialIndex;

    return output;
}

// ==================== Fragment Shader ====================
struct FragmentOutput {
    float4 color : SV_Target0;
};

[[vk::binding(0, 0)]]
SamplerState samplers[];
[[vk::binding(1, 0)]]
Texture2D textures[];

[shader("fragment")]
FragmentOutput fragmentMain(
    VertexOutput input,
    uniform PushConsts pushConstants)
{
    MaterialProperties material = pushConstants.materialBuffer->materials[input.materialIndex];

    float3 N = normalize(input.normal);
    float3 V = normalize(pushConstants.sceneData->cameraWorldPos.xyz - input.worldPos);
    float3 L = normalize(float3(1.0, 1.0, 1.0));

    float3 ambient = 0.1;
    float3 diffuse = max(dot(N, L), 0.0) * material.colorFactor.rgb;

    FragmentOutput output;
    output.color = float4(ambient + diffuse, material.colorFactor.a);

    return output;
}