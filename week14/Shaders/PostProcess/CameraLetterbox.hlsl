#include "Common/Functions.hlsli"

cbuffer CameraLetterboxCB : register(b2)
{
    float4 LetterboxColor;
    float LetterboxAmount;
    float LetterboxThickness;
    float2 _Pad;
};

PS_Input_UV VS(uint vertexID : SV_VertexID)
{
    return FullscreenTriangleVS(vertexID);
}

float4 PS(PS_Input_UV input) : SV_TARGET
{
    float thickness = saturate(LetterboxThickness * LetterboxAmount);
    
    bool inTopBar = input.uv.y < thickness;
    bool inBottomBar = input.uv.y > 1.0 - thickness;
    
    if (!inTopBar && !inBottomBar)
    {
        discard;
    }
    
    return LetterboxColor;
}