#include "ShaderCommon.hlsli"

cbuffer MaterialData : register(b2)
{
	float4 BaseColor; 
	float2 UVScrollSpeed; 
	float2 Padding1;
	float4 WaveData; 
};

VS_OUTPUT main(VS_INPUT Input)
{
	VS_OUTPUT Output;
    
	float4 WorldPos = mul(float4(Input.Position, 1.0f), World);
	float4 ViewPos = mul(WorldPos, View);
	Output.Position = mul(ViewPos, Projection);
    
	Output.Color = Input.Color * BaseColor;
	Output.Normal = mul(Input.Normal, (float3x3) World);
    
	float uvWaveX = sin(Input.UV.y * WaveData.y + Time * WaveData.z) * WaveData.x;
	float uvWaveY = cos(Input.UV.x * WaveData.y + Time * WaveData.z) * WaveData.x;
    
	Output.UV = Input.UV + (UVScrollSpeed * Time) + float2(uvWaveX, uvWaveY);
    
	return Output;
}