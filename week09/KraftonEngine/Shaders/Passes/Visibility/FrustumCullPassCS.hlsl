// Shader: FrustumCullPassCS
// Role: frustum culling compute entry.
// Entry: CS.
// Slots: pass-local unless declared in included common headers.

RWStructuredBuffer<uint> g_VisibleInstances : register(u0);

[numthreads(64, 1, 1)]
void CS(uint3 DTid : SV_DispatchThreadID)
{
    g_VisibleInstances[DTid.x] = 0u;
}

