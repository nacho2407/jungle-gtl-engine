#include "Common/Functions.hlsli"
#include "Common/SystemSamplers.hlsli"

// Mode: 0 = Linear (1-d)*Brightness, 1 = Pow pow(1-d, Exponent)
cbuffer ShadowVisCB : register(b2)
{
	float2 UVMin;
	float2 UVMax;
	float  Brightness;
	uint   SliceIndex;
	uint   bIsTextureArray;
	uint   Mode;
	float  Exponent;
	float3 _pad;
};

Texture2D<float>      ShadowTex2D  : register(t0);
Texture2DArray<float> ShadowTexArr : register(t1);

PS_Input_UV VS(uint vid : SV_VertexID) { return FullscreenTriangleVS(vid); }

float4 PS(PS_Input_UV input) : SV_Target
{
	float2 uv = lerp(UVMin, UVMax, input.uv);
	float d;
	if (bIsTextureArray)
		d = ShadowTexArr.Sample(LinearClampSampler, float3(uv, SliceIndex));
	else
		d = ShadowTex2D.Sample(LinearClampSampler, uv);
	float v;
	if (Mode == 1)
		v = pow(saturate(1.0 - d), Exponent) * Brightness;
	else
		v = (1.0 - d) * Brightness;
	return float4(v, v, v, 1.0);
}
