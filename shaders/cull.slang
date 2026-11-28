struct ChunkCullData {
    float4 originScale;
    uint indexCount;
    uint firstIndex;
    int vertexOffset;
    uint pad;
};

struct DrawCommand {
    uint indexCount;
    uint instanceCount;
    uint firstIndex;
    int vertexOffset;
    uint firstInstance;
};

struct CullPushConstants {
    float4 planes[6];
    uint slotCount;
    uint outputBase;
    uint countIndex;
    uint maxDraws;
    float chunkExtent;
};
[[vk::push_constant]]
CullPushConstants push;

[[vk::binding(0, 0)]] StructuredBuffer<ChunkCullData> chunks;
[[vk::binding(1, 0)]] RWStructuredBuffer<DrawCommand> commands;
[[vk::binding(2, 0)]] RWStructuredBuffer<float4> instances;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> counts;

bool isVisible(float3 aabbMin, float3 aabbMax) {
    for (uint i = 0; i < 6; i++) {
        float3 normal = push.planes[i].xyz;
        float3 p = float3(
            normal.x >= 0.0 ? aabbMax.x : aabbMin.x,
            normal.y >= 0.0 ? aabbMax.y : aabbMin.y,
            normal.z >= 0.0 ? aabbMax.z : aabbMin.z);

        if (dot(normal, p) + push.planes[i].w < 0.0)
            return false;
    }

    return true;
}

[shader("compute")]
[numthreads(64, 1, 1)]
void cullMain(uint3 threadId : SV_DispatchThreadID) {
    uint slot = threadId.x;

    if (slot >= push.slotCount)
        return;

    ChunkCullData chunk = chunks[slot];

    if (chunk.indexCount == 0)
        return;

    float3 aabbMin = chunk.originScale.xyz;
    float3 aabbMax = aabbMin + push.chunkExtent;

    if (!isVisible(aabbMin, aabbMax))
        return;

    uint drawId;
    InterlockedAdd(counts[push.countIndex], 1, drawId);

    if (drawId >= push.maxDraws)
        return;

    uint outputId = push.outputBase + drawId;

    instances[outputId] = chunk.originScale;

    DrawCommand command;
    command.indexCount = chunk.indexCount;
    command.instanceCount = 1;
    command.firstIndex = chunk.firstIndex;
    command.vertexOffset = chunk.vertexOffset;
    command.firstInstance = outputId;
    commands[outputId] = command;
}
