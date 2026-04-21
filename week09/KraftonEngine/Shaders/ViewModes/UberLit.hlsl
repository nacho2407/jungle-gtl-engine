#include "../Common/Types/CommonTypes.hlsli"   // Functions.hlsl → ConstantBuffers.hlsl (b0/b1/b4, FLocalLightInfo)
#include "../Common/Types/SurfaceData.hlsli"
#include "../Common/Types/LightingCommon.hlsli" // t6 StructuredBuffer<FLocalLightInfo>, 조명 함수들

#define TILE_SIZE                       4
#define NUM_SLICES                      32
#define MAX_LIGHTS_PER_TILE             1024
#define SHADER_ENTITY_TILE_BUCKET_COUNT (MAX_LIGHTS_PER_TILE / 32)   // 32

cbuffer LightCullingParams : register(b2)
{
    uint2 ScreenSize;
    uint2 TileSize; // == TILE_SIZE

    uint Enable25DCulling;
    float NearZ;
    float FarZ;
    float NumLights;
};

Texture2D g_BaseColorTex : register(t0);
Texture2D g_Surface1Tex : register(t1);
Texture2D g_Surface2Tex : register(t2);
Texture2D g_ModifiedBaseColorTex : register(t3);
Texture2D g_ModifiedSurface1Tex : register(t4);
Texture2D g_ModifiedSurface2Tex : register(t5);
StructuredBuffer<FLocalLightInfo> g_LightBuffer : register(t6);
StructuredBuffer<uint> PerTileLightMask : register(t7);

float4 ResolveBaseColor(float2 UV)
{
    float4 BaseColor = g_BaseColorTex.Sample(LinearClampSampler, UV);   // 원본 base color
    float4 ModifiedBaseColor = g_ModifiedBaseColorTex.Sample(LinearClampSampler, UV);   //데칼 수정본
    return ResolveSurfaceValue(BaseColor, ModifiedBaseColor);
}

float4 ResolveSurface1(float2 UV)
{
    float4 Surface1 = g_Surface1Tex.Sample(LinearClampSampler, UV);
    float4 ModifiedSurface1 = g_ModifiedSurface1Tex.Sample(LinearClampSampler, UV);
    return ResolveSurfaceValue(Surface1, ModifiedSurface1);
}

float4 ResolveSurface2(float2 UV)
{
    float4 Surface2 = g_Surface2Tex.Sample(LinearClampSampler, UV);
    float4 ModifiedSurface2 = g_ModifiedSurface2Tex.Sample(LinearClampSampler, UV);
    return ResolveSurfaceValue(Surface2, ModifiedSurface2);
}

PS_Input_UV VS_Fullscreen(uint VertexID : SV_VertexID)
{
    return FullscreenTriangleVS(VertexID);
}

float4 PS_UberLit(PS_Input_UV Input) : SV_TARGET0
{
    float2 UV = Input.uv;
    float4 BaseColor = ResolveBaseColor(UV);

    uint2 PixelCoord = uint2(Input.position.xy);
    uint2 TileCoord = PixelCoord / TileSize; // 각 성분별 나눔
    uint TilesX = ScreenSize.x / TileSize.x; // 한 행에 존재하는 타일 수
    uint FlatTileIndex = TileCoord.x + TileCoord.y * TilesX;
    
    float4 FinalColor = BaseColor;
    
#if defined(LIGHTING_MODEL_GOURAUD)
    // Gouraud는 Surface1에 인코딩되지 않은 라이팅 값이 들어있으므로 직접 사용
    float4 GouraudL = ResolveSurface1(UV);
    FinalColor = ComputeGouraudLighting(BaseColor, GouraudL);
    
//Lihgtculling 적용대상
#elif defined(LIGHTING_MODEL_LAMBERT)
    //Directional Light
    float3 Normal = DecodeNormal(ResolveSurface1(UV));
    FinalColor = ComputeLambertLighting(BaseColor, Normal);
    
    //LocalPointLight && Light Culling
    uint TileIndex = FlatTileIndex;
    int BucketsPerTile = MAX_LIGHTS_PER_TILE / 32;
    int StartIndex = TileIndex * BucketsPerTile;
    //for (int Bucket = 0; Bucket < BucketsPerTile; ++Bucket) 
    //{
    //    int PointMask = PerTileLightMask[StartIndex + Bucket];
    //    for (int bit = 0; bit < 32; ++bit) 
    //    {
    //        if (PointMask & (1u << bit)) 
    //        {
    //            // 전역 조명 인덱스는 bucket * 32 + bit 로 계산됨.
    //            // 예외처리 - 전역 조명 인덱스가 총 조명 수보다 작은 경우에만 라이팅 계산
    //            int GlobalPointLightIndex = Bucket * 32 + bit;
    //            if (GlobalPointLightIndex < MAX_LIGHTS_PER_TILE)
    //            {
    //                FLocalLightInfo LocalLight = g_LightBuffer[ GlobalPointLightIndex];
    //                FinalColor += LocalLightLambert(LocalLight, Normal, BaseColor, UV);
    //            }
    //        }
    //    }
    //}
    

    for (int i = 0; i < NumLocalLights; ++i) 
    {
 
        FLocalLightInfo LocalLight = g_LightBuffer[i];
        FinalColor+= LocalLightLambert(LocalLight, Normal, BaseColor, UV);

    }
    
//Lihgtculling 적용대상
#elif defined(LIGHTING_MODEL_PHONG)
    float3 Normal = DecodeNormal(ResolveSurface1(UV));
    float4 MaterialParam = DecodeMaterialParam(ResolveSurface2(UV));
    FinalColor = ComputeBlinnPhongLighting(BaseColor, Normal, MaterialParam, UV);
    
#elif defined(LIGHTING_MODEL_WORLDNORMAL)
    float3 Normal = DecodeNormal(ResolveSurface1(UV));
    outputColor = float4(Normal * 0.5f + 0.5f, 1.0f);
#else
    FinalColor = BaseColor;
#endif
    
    return FinalColor;
}
