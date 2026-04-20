RWStructuredBuffer<uint> g_InstanceVisibility : register(u0);

[numthreads(64, 1, 1)]
void CS(uint3 DTid : SV_DispatchThreadID)
{
    g_InstanceVisibility[DTid.x] = 0u;
}
