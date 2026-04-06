#include "ShaderCommon.hlsli"

Texture2D<float> InputHZB : register(t0);
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

	const uint2 DstCoord = DispatchThreadId.xy;
	const uint2 SrcBase = DstCoord * 2;

	float MaxDepth = 0.0f;

	[unroll]
	for (uint OffsetY = 0; OffsetY < 2; ++OffsetY)
	{
		[unroll]
		for (uint OffsetX = 0; OffsetX < 2; ++OffsetX)
		{
			const uint2 SrcCoord = min(SrcBase + uint2(OffsetX, OffsetY), uint2(SourceWidth - 1, SourceHeight - 1));
			MaxDepth = max(MaxDepth, InputHZB.Load(int3(SrcCoord, 0)));
		}
	}

	OutputHZB[DstCoord] = MaxDepth;
}
