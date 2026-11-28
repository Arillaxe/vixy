struct VSInput {
    [[vk::location(0)]] float3 inPosition;
    [[vk::location(1)]] float2 inTexCoord;
    [[vk::location(2)]] float3 normal;
    [[vk::location(3)]] uint32_t textureId;
};

struct ChunkVSInput {
    [[vk::location(0)]] uint32_t data;
    [[vk::location(1)]] uint32_t textureId;
    [[vk::location(2)]] float4 originScale;
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

static const float3 faceNormals[6] = {
    float3(1, 0, 0),
    float3(-1, 0, 0),
    float3(0, 1, 0),
    float3(0, -1, 0),
    float3(0, 0, 1),
    float3(0, 0, -1),
};

[shader("vertex")]
VSOutput vertMain(ChunkVSInput input) {
    VSOutput output;

    uint data = input.data;
    float3 local = float3(data & 31, (data >> 5) & 31, (data >> 10) & 31);
    float2 uv = float2((data >> 15) & 31, (data >> 20) & 31);
    float3 normal = faceNormals[(data >> 25) & 7];

    float3 worldPos = input.originScale.xyz + local * input.originScale.w;

    output.pos = mul(ubo.proj, mul(ubo.view, float4(worldPos, 1)));
    output.fragTexCoord = uv;
    output.color = max(dot(normal, sunDir), 0.2);
    output.textureId = input.textureId;

    return output;
}

[shader("vertex")]
VSOutput entityVertMain(VSInput input) {
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
