struct VSInput {
    float3 inPosition;
    float2 inTexCoord;
    float3 normal;
    uint32_t textureId;
};

struct UniformBuffer {
    float4x4 view;
    float4x4 proj;
    float3 cameraPosition;
    float deltaTime;
};
ConstantBuffer<UniformBuffer> ubo;

struct VSOutput {
    float4 pos : SV_Position;
    float2 fragTexCoord;
    float4 color;
    nointerpolation uint32_t textureId;
};

struct PushConstants {
    float4x4 model;
};
[[vk::push_constant]]
PushConstants push;

uniform Sampler2D textures[4096];

static float3 sunDir = normalize(float3(0.5, 1.0, 0.3));

[shader("vertex")]
VSOutput vertMain(VSInput input, uint vertexId: SV_VertexID) {
    VSOutput output;

    output.pos = mul(ubo.proj, mul(ubo.view, mul(push.model, float4(input.inPosition, 1))));
    output.fragTexCoord = input.inTexCoord;
    output.color = max(dot(input.normal, sunDir), 0.2);
    output.textureId = input.textureId;

    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_Target {
    return textures[NonUniformResourceIndex(vertIn.textureId)].Sample(vertIn.fragTexCoord) * vertIn.color;
}
