#include "../Common/Common.hlsli"

cbuffer OutlineConstants : register(b2)
{
    float4 OutlineColor;
    float OutlineThicknessPixels;
    float2 OutlineViewportSize;
    float2 OutlineViewportOrigin;
    float2 OutlinePadding0;
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
    const int2 viewportOrigin = int2(OutlineViewportOrigin);
    const int2 pixelCoordAbs = int2(input.position.xy);
    const int2 pixelCoordLocal = pixelCoordAbs - viewportOrigin;

	//	Subviewport local 踰붿쐞 ??clamp ?? ?ㅼ떆 absolute 醫뚰몴濡??섏궛
    const int2 clampedLocal = clamp(pixelCoordLocal, int2(0, 0), viewportSize - 1);
    const int2 clampedAbs = clampedLocal + viewportOrigin;

    const float centerMask = SelectionMaskTexture.Load(int3(clampedAbs, 0));
	
    //	留뚯씪 0.5f ?댁긽?대씪??寃껋? Mask ?먯껜?쇰뒗 寃?
    if (centerMask > 0.5f)
    {
        discard;
    }

	//	OutlineThicknessPixel??integer濡??ъ슜?????덈룄濡?round 諛?理쒖냼 1 蹂댁옣 (紐??쎌?源뚯? 寃?ы븷 寃껋씤吏)
    const int radius = max((int)round(OutlineThicknessPixels), 1);
    
    const int2 neighborOffsets[8] =
    {
        int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1),
        int2(-1, -1), int2(-1, 1), int2(1, -1), int2(1, 1)
    };

	//	嫄곕━ 痢≪젙 (Mask濡쒕????⑥뼱吏?嫄곕━)
    float minDist = radius + 1;

    for (int r = 1; r <= radius; ++r)
    {
        [unroll]
        for (int i = 0; i < 8; ++i)
        {
            const int2 sampleLocal = clamp(clampedLocal + neighborOffsets[i] * r, int2(0, 0), viewportSize - 1);
            const int2 sampleAbs = sampleLocal + viewportOrigin;
            float mask = SelectionMaskTexture.Load(int3(sampleAbs, 0));
    
			//	?좏깮???쎌??몄? 0, 1濡?check
            float hit = step(0.5f, mask);
			//	lerp(a, b, t) = a * (1 - t) + b * t (遺꾧린瑜??쒓굅?섍린 ?꾪빐 lerp ?ъ슜)
            float dist = lerp(9999.0f, (float)r, hit);
            minDist = min(minDist, dist);
        }
    }
    
    if (minDist <= radius)
    {
        float t = 1.0f - ((minDist - 1.0f) / radius);
        t = saturate(t);	//	0 ~ 1 clamp
        t = t * t;			//	Linear?섏? ?딄쾶 (??遺?쒕읇寃?泥섎━) - 媛먮쭏 怨≪꽑
    
        return float4(OutlineColor.rgb, OutlineColor.a * t);
    }

    discard;
	return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
