#include "ShaderCommon.hlsli"

Texture2D<float> InputDepth : register(t0);
RWTexture2D<float> OutputHZB : register(u0);

cbuffer HZBBuildData : register(b0)
{
	uint SourceWidth;
	uint SourceHeight;
	uint DestWidth;
	uint DestHeight;
};

[numthreads(8, 8, 1)]
void main(uint3 DispatchThreadId : SV_DispatchThreadID)
{
	if (DispatchThreadId.x >= DestWidth || DispatchThreadId.y >= DestHeight)
	{
		return;
	}

	const uint2 Coord = DispatchThreadId.xy;
	OutputHZB[Coord] = InputDepth.Load(int3(Coord, 0));
}
