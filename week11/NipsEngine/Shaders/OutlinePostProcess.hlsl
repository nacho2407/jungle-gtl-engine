#include "Common.hlsl"

cbuffer OutlineConstants : register(b2)
{
    float4 OutlineColor;
    float OutlineThicknessPixels;
    float2 OutlineViewportSize;
    float OutlinePadding0;
};

Texture2D<float> SelectionMaskTexture : register(t7);

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VS(uint vertexId : SV_VertexID)
{
    VSOutput output;

    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f, 3.0f),
        float2(3.0f, -1.0f)
    };

    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    return output;
}

float4 PS(VSOutput input) : SV_TARGET
{
    const int2 viewportSize = int2(max(OutlineViewportSize, float2(1.0f, 1.0f)));
    const int2 pixelCoord = int2(input.position.xy);
    
	//	Viewport 범위 내 Clamp
    const int2 clampedCoord = clamp(pixelCoord, int2(0, 0), viewportSize - 1);

    const float centerMask = SelectionMaskTexture.Load(int3(clampedCoord, 0));
	
    //	만일 0.5f 이상이라는 것은 Mask 자체라는 것
    if (centerMask > 0.5f)
    {
        discard;
    }

	//	OutlineThicknessPixel이 integer로 사용될 수 있도록 round 및 최소 1 보장 (몇 픽셀까지 검사할 것인지)
    const int radius = max((int)round(OutlineThicknessPixels), 1);
    
    const int2 neighborOffsets[8] =
    {
        int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1),
        int2(-1, -1), int2(-1, 1), int2(1, -1), int2(1, 1)
    };

	//	거리 측정 (Mask로부터 떨어진 거리)
    float minDist = radius + 1;

    for (int r = 1; r <= radius; ++r)
    {
        [unroll]
        for (int i = 0; i < 8; ++i)
        {
            const int2 sampleCoord = clamp(clampedCoord + neighborOffsets[i] * r, int2(0, 0), viewportSize - 1);
            float mask = SelectionMaskTexture.Load(int3(sampleCoord, 0));
    
			//	선택된 픽셀인지 0, 1로 check
            float hit = step(0.5f, mask);
			//	lerp(a, b, t) = a * (1 - t) + b * t (분기를 제거하기 위해 lerp 사용)
            float dist = lerp(9999.0f, (float)r, hit);
            minDist = min(minDist, dist);
        }
    }
    
    if (minDist <= radius)
    {
        float t = 1.0f - ((minDist - 1.0f) / radius);
        t = saturate(t);	//	0 ~ 1 clamp
        t = t * t;			//	Linear하지 않게 (더 부드럽게 처리) - 감마 곡선
    
        return float4(OutlineColor.rgb, OutlineColor.a * t);
    }

    discard;
	return float4(0.0f, 0.0f, 0.0f, 1.0f);
}