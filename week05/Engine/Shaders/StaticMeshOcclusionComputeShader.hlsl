#define OCCLUSION_NUM_THREADS 64

struct FGpuOcclusionCandidate
{
	float3 BoundsCenter;
	float BoundsRadius;
	float3 BoundsExtent;
	uint DenseIndex;
};

cbuffer OcclusionPassData : register(b0)
{
	float4x4 View;
	float4x4 Projection;
	float4x4 ViewProjection;

	uint ViewWidth;
	uint ViewHeight;
	uint CandidateCount;
	uint HZBMipCount;

	float DepthBias;
	float NearPlaneEpsilon;
	float2 Padding;
};

Texture2D<float> HZBTexture : register(t0);
StructuredBuffer<FGpuOcclusionCandidate> Candidates : register(t1);
RWStructuredBuffer<uint> VisibilityResults : register(u0);

uint2 GetMipDimensions(uint MipIndex)
{
	return uint2(max(ViewWidth >> MipIndex, 1u), max(ViewHeight >> MipIndex, 1u));
}

float SampleHZBAtMip(float2 UV, uint MipIndex)
{
	const uint2 MipDimensions = GetMipDimensions(MipIndex);
	const float2 ClampedUV = saturate(UV);
	const uint2 TexelCoord = min((uint2)(ClampedUV * (float2)MipDimensions), MipDimensions - 1);
	return HZBTexture.Load(int3((int2)TexelCoord, (int)MipIndex));
}

bool ProjectCandidateSphere(FGpuOcclusionCandidate Candidate, out float2 OutMinUV, out float2 OutMaxUV, out float OutNearestDepth)
{
	const float3 Center = Candidate.BoundsCenter;
	const float Radius = Candidate.BoundsRadius;
	
	const float4 ViewCenter4 = mul(float4(Center, 1.0f), View);
	const float3 ViewCenter = ViewCenter4.xyz;
	
	if (ViewCenter.x <= NearPlaneEpsilon)
	{
		return false;
	}
	
	const float4 ClipCenter = mul(float4(ViewCenter, 1.0f), Projection);
	if (ClipCenter.w <= NearPlaneEpsilon)
	{
		return false;
	}

	const float3 NdcCenter3 = ClipCenter.xyz / ClipCenter.w;
	
	const float4 ClipRight = mul(float4(ViewCenter.x, ViewCenter.y + Radius, ViewCenter.z, 1.0f), Projection);
	const float4 ClipUp = mul(float4(ViewCenter.x, ViewCenter.y, ViewCenter.z + Radius, 1.0f), Projection);

	if (ClipRight.w <= NearPlaneEpsilon || ClipUp.w <= NearPlaneEpsilon)
	{
		return false;
	}

	const float2 NdcCenter = NdcCenter3.xy;
	const float2 NdcRight = ClipRight.xy / ClipRight.w;
	const float2 NdcUp = ClipUp.xy / ClipUp.w;

	const float RadiusNdcX = abs(NdcRight.x - NdcCenter.x);
	const float RadiusNdcY = abs(NdcUp.y - NdcCenter.y);
	
	const float2 CenterUV = float2(
        NdcCenter.x * 0.5f + 0.5f,
        -NdcCenter.y * 0.5f + 0.5f
    );

	const float2 RadiusUV = float2(RadiusNdcX, RadiusNdcY) * 0.5f;

	OutMinUV = saturate(CenterUV - RadiusUV);
	OutMaxUV = saturate(CenterUV + RadiusUV);
	
	const float NearestViewX = max(ViewCenter.x - Radius, NearPlaneEpsilon);
	const float4 ClipNearest = mul(float4(NearestViewX, ViewCenter.y, ViewCenter.z, 1.0f), Projection);
	OutNearestDepth = saturate(ClipNearest.z / ClipNearest.w);

	if (OutMinUV.x >= OutMaxUV.x || OutMinUV.y >= OutMaxUV.y)
	{
		return false;
	}

	return true;
}

[numthreads(OCCLUSION_NUM_THREADS, 1, 1)]
void main(uint3 DispatchThreadID : SV_DispatchThreadID)
{
	const uint CandidateIndex = DispatchThreadID.x;
	if (CandidateIndex >= CandidateCount)
	{
		return;
	}

	const FGpuOcclusionCandidate Candidate = Candidates[CandidateIndex];
	if (HZBMipCount == 0)
	{
		VisibilityResults[Candidate.DenseIndex] = 1;
		return;
	}

	float2 MinUV = 0.0f.xx;
	float2 MaxUV = 0.0f.xx;
	float NearestDepth = 0.0f;
	if (!ProjectCandidateSphere(Candidate, MinUV, MaxUV, NearestDepth))
	{
		VisibilityResults[Candidate.DenseIndex] = 1;
		return;
	}

	const float2 ScreenRectPixels = max((MaxUV - MinUV) * float2((float) ViewWidth, (float) ViewHeight), 1.0f.xx);
	const float MaxRectExtent = max(ScreenRectPixels.x, ScreenRectPixels.y);

	uint MipIndex = 0u;
	if (MaxRectExtent > 1.0f)
	{
		const uint BaseMip = min(HZBMipCount - 1, (uint) floor(log2(MaxRectExtent)));
		MipIndex = (BaseMip > 0u) ? (BaseMip - 1u) : 0u;
	}
	
	if (MaxRectExtent <= 8.0f)
	{
		MipIndex = 0u;
	}
	
	const float2 CenterUV = 0.5f * (MinUV + MaxUV);
	const float2 HalfSizeUV = 0.5f * (MaxUV - MinUV);
	
	const float2 OffsetX = float2(HalfSizeUV.x * 0.6f, 0.0f);
	const float2 OffsetY = float2(0.0f, HalfSizeUV.y * 0.6f);

	float HZBMaxDepth = 0.0f;
	HZBMaxDepth = max(HZBMaxDepth, SampleHZBAtMip(CenterUV, MipIndex));
	HZBMaxDepth = max(HZBMaxDepth, SampleHZBAtMip(CenterUV + OffsetX, MipIndex));
	HZBMaxDepth = max(HZBMaxDepth, SampleHZBAtMip(CenterUV - OffsetX, MipIndex));
	HZBMaxDepth = max(HZBMaxDepth, SampleHZBAtMip(CenterUV + OffsetY, MipIndex));
	HZBMaxDepth = max(HZBMaxDepth, SampleHZBAtMip(CenterUV - OffsetY, MipIndex));

	const uint bVisible = (NearestDepth <= (HZBMaxDepth + DepthBias)) ? 1u : 0u;
	VisibilityResults[Candidate.DenseIndex] = bVisible;
}
