// b0: 프레임당 1회 (카메라)
cbuffer FrameData : register(b0)
{
	float4x4 View;
	float4x4 Projection;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION; // 클립 공간 좌표
	float3 WorldPos : TEXCOORD0; // 월드 공간 좌표
	float3 LocalPos : TEXCOORD1;
	int PlaneNo : TEXCOORD2;
};

// ==========================================
// Vertex Shader
// ==========================================
VS_OUTPUT main(uint id : SV_VertexID)
{
	VS_OUTPUT output;

	float3 positions[18] =
	{
		float3(-1.0f, -1.0f, 0.0f),
        float3(1.0f, -1.0f, 0.0f),
        float3(-1.0f, 1.0f, 0.0f),
        
        float3(1.0f, -1.0f, 0.0f),
        float3(-1.0f, 1.0f, 0.0f),
        float3(1.0f, 1.0f, 0.0f),
        
        float3(0.0f, -0.0001f, -1.0f),
        float3(0.0f, 0.0001f, -1.0f),
        float3(0.0f, -0.0001f, 1.0f),
        
        float3(0.0f, 0.0001f, -1.0f),
        float3(0.0f, -0.0001f, 1.0f),
        float3(0.0f, 0.0001f, 1.0f),
        
        float3(-0.0001f, 0.0f, -1.0f),
        float3(0.0001f, 0.0f, -1.0f),
        float3(-0.0001f, 0.0f, 1.0f),
        
        float3(0.0001f, 0.0f, -1.0f),
        float3(-0.0001f, 0.0f, 1.0f),
        float3(0.0001f, 0.0f, 1.0f)
	};
	int planeno[3] = { 0, 1, 2 };
    
	float range = 1000.0f;
	float3 lPos = positions[id];
	output.PlaneNo = planeno[id / 6];
	output.LocalPos = lPos;

	float3 worldPosition = lPos * range;
	output.WorldPos = worldPosition;
	output.Pos = mul(float4(worldPosition, 1.0f), mul(View, Projection));
    
	return output;
}
