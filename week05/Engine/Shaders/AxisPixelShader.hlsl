cbuffer FrameData : register(b0)
{
	float4x4 View;
	float4x4 Projection;
};

cbuffer CameraBuffer : register(b2)
{
	float3 _UnusedCameraPos; // 렌더러에서 더 이상 넘기지 않아도 됨
	float GridSize;
	float LineThickness;
	float3 padding;
};

struct VS_OUTPUT
{
	float4 Pos : SV_POSITION; // 클립 공간 좌표
	float3 WorldPos : TEXCOORD0; // 월드 공간 좌표
	float3 LocalPos : TEXCOORD1;
	int PlaneNo : TEXCOORD2;
};

// View 행렬에서 카메라 월드 좌표 추출
float3 GetCameraPos()
{
	// View = [R | T] (Row-major transposed for HLSL = Column-major)
	// CameraPos = - (R^T * T)
	float3x3 R = (float3x3)View;
	float3 T = float3(View[0][3], View[1][3], View[2][3]);
	return -mul(R, T);
}

// ==========================================
// Pixel Shader
// ==========================================
float4 main(VS_OUTPUT input) : SV_Target
{
	float3 CameraPos = GetCameraPos();
	float dist = distance(input.WorldPos, CameraPos);
    
	float maxDistance = 1000.0f;
	if (input.PlaneNo == 0)
	{
		float2 derivative = fwidth(input.WorldPos.xy);
		float2 coord = input.WorldPos.xy / GridSize;
    
		float2 grid = abs(frac(coord - 0.5f) - 0.5f) / max(derivative / GridSize, 0.0001f);
		float lineAlpha = saturate(LineThickness - min(grid.x, grid.y));

		float2 axisDrawWidth = derivative * 1.5f;
		float axisX = 1.0f - smoothstep(0.0f, axisDrawWidth.x + 0.001f, abs(input.WorldPos.x));
		float axisY = 1.0f - smoothstep(0.0f, axisDrawWidth.y + 0.001f, abs(input.WorldPos.y));
        
		float3 color = float3(0.5f, 0.5f, 0.5f);
		color = lerp(color, float3(0.2f, 1.0f, 0.2f), axisX); // Green (Y축 방향 선)
		color = lerp(color, float3(1.0f, 0.2f, 0.2f), axisY); // Red (X축 방향 선)

		float fade = pow(saturate(1.0f - dist / maxDistance), 2.0f);
		float finalAlpha = max(lineAlpha, max(axisX, axisY)) * fade;

		if (finalAlpha < 0.01f)
			discard;
		return float4(color, finalAlpha);
	}
	else
	{
		float WorldCoord = (input.PlaneNo == 1) ? input.WorldPos.y : input.WorldPos.x;
		float derivative = fwidth(WorldCoord);
		float axisZ = 1.0f - smoothstep(0.0f, derivative * 1.5f, abs(WorldCoord));

		float3 color = float3(0.2f, 0.2f, 1.0f); // Z축 표준 색상: Blue
    
		float fade = pow(saturate(1.0f - dist / maxDistance), 2.0f);
		float finalAlpha = axisZ * fade;

		if (finalAlpha < 0.01f)
			discard;

		return float4(color, finalAlpha);
	}
}
