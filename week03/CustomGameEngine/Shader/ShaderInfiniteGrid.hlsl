cbuffer FrameConstant : register(b0)
{
    row_major matrix View;
    row_major matrix Projection;
};

cbuffer CameraBuffer : register(b2)
{
    float3 CameraPos;
    float GridSize;
    float LineThickness;
    float3 padding;
};

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION; // 클립 공간 좌표
    float3 WorldPos : TEXCOORD0; // 월드 공간 좌표
};

// ==========================================
// Vertex Shader
// ==========================================
VS_OUTPUT mainVS(uint id : SV_VertexID)
{
    VS_OUTPUT output;

    float2 positions[4] =
    {
        float2(-10000.0f, -10000.0f),
        float2(10000.0f, -10000.0f),
        float2(-10000.0f, 10000.0f),
        float2(10000.0f, 10000.0f)
    };
    float4 worldPosition = float4(positions[id].x, positions[id].y, 0.0f, 1.0f);
    matrix viewProj = mul(View, Projection);
    output.Pos = mul(worldPosition, viewProj);
    output.WorldPos = worldPosition.xyz;

    return output;
}

// ==========================================
// Pixel Shader
// ==========================================
float4 mainPS(VS_OUTPUT input) : SV_Target
{
    float dist = distance(input.WorldPos, CameraPos);
    if (dist < 0.5)
        discard;
    
    float2 coord = input.WorldPos.xy / GridSize;
    float2 derivative = fwidth(coord);
    
    float2 grid = abs(frac(coord - 0.5f) - 0.5f) / max(derivative, 0.00001f);
    float lineAlpha = clamp(LineThickness - min(grid.x, grid.y), 0.0f, 1.0f);

    float2 axisDrawWidth = derivative * GridSize * 1.5f;
    float axisX = 1.0f - smoothstep(0.0f, axisDrawWidth.x + 0.001f, abs(input.WorldPos.x));
    float axisY = 1.0f - smoothstep(0.0f, axisDrawWidth.y + 0.001f, abs(input.WorldPos.y));

    float3 color = float3(0.5f, 0.5f, 0.5f);
    color = lerp(color, float3(0.2f, 1.0f, 0.2f), axisX); // Green (Y축 방향 선)
    color = lerp(color, float3(1.0f, 0.2f, 0.2f), axisY); // Red (X축 방향 선)

    float maxDistance = 10000.0f;
    float fade = pow(saturate(1.0f - dist / maxDistance), 2.0f);
    float lodFade = saturate(0.8f - max(derivative.x, derivative.y));
    float finalAlpha = max(lineAlpha, max(axisX, axisY)) * fade * lodFade;

    if (finalAlpha < 0.01f)
        discard;
    return float4(color, finalAlpha);
}
