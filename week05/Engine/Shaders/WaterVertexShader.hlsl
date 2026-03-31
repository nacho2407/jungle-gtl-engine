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
    
	float waveX = sin(Input.Position.x * WaveData.y + Time * WaveData.z);
	float waveZ = cos(Input.Position.z * WaveData.y + Time * WaveData.z);
    
	float3 NewPosition = Input.Position;
	NewPosition.y += (waveX + waveZ) * WaveData.x; 
	
	float4 WorldPos = mul(float4(NewPosition, 1.0f), World);
	float4 ViewPos = mul(WorldPos, View);
	Output.Position = mul(ViewPos, Projection);
    
	Output.Color = Input.Color * BaseColor;
    
    // 노말 맵을 쓰지 않는다면 일단 기존 노말을 그대로 넘깁니다.
	Output.Normal = mul(Input.Normal, (float3x3) World);
    
	Output.UV = Input.UV + (UVScrollSpeed * Time);
    
	return Output;
}