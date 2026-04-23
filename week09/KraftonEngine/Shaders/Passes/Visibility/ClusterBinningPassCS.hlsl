// Shader: ClusterBinningPassCS
// Role: visibility cluster binning compute entry.
// Entry: CS.
// Slots: pass-local unless declared in included common headers.

#include "../../Common/Resources/ConstantBuffers.hlsl"

RWStructuredBuffer<uint> g_ClusterBins : register(u0);

[numthreads(8, 8, 1)]
void CS(uint3 DTid : SV_DispatchThreadID)
{
    const uint LinearIndex = DTid.x + DTid.y * 1024u;
    g_ClusterBins[LinearIndex] = 0u;
}

