import common;

struct VertexOutput {
    float4 position : SV_Position;
    float3 worldPos : WORLD_POS;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    uint materialIndex : MATERIAL_INDEX;
};


struct PushConsts {
    SceneData* sceneData;

    MaterialData* materialBuffer;

    TraditionalPrimitiveData* primitiveBuffer;
    ModelData* modelBuffer;
    InstanceData* instanceBuffer;
};


// ==================== Vertex Shader ====================
[shader("vertex")]
VertexOutput vertexMain(
    Vertex inVertex,
    uniform PushConsts pushConstants,
    uint instanceIndex : SV_InstanceID)
{
    Instance instance = pushConstants.instanceBuffer->instances[instanceIndex];
    Model model = pushConstants.modelBuffer->models[instance.modelIndex];
    TraditionalPrimitive primitive = pushConstants.primitiveBuffer->primitives[instance.primitiveIndex];

    float4 worldPos = mul(model.modelMatrix, float4(inVertex.position, 1.0));
    float3 worldNormal = normalize(mul((float3x3)model.modelMatrix, inVertex.normal));
    float3 worldTangent = normalize(mul((float3x3)model.modelMatrix, inVertex.tangent.xyz));

    VertexOutput output;
    output.position = mul(pushConstants.sceneData->viewProj, worldPos);
    output.worldPos = worldPos.xyz;
    output.normal = worldNormal;
    output.tangent = float4(worldTangent, inVertex.tangent.w);
    output.uv = inVertex.uv;
    output.color = inVertex.color;
    output.materialIndex = primitive.materialIndex;

    return output;
}

// ==================== Fragment Shader ====================
struct FragmentOutput {
    float4 color : SV_Target0;
};

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