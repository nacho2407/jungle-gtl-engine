cbuffer ScreenOverlayCB : register(b13)
{
    float4 OverlayColor;
}

struct VSOutput
{
    float4 ClipPos : SV_POSITION;
};

VSOutput mainVS(uint vertexID : SV_VertexID)
{
    float2 pos;
    if (vertexID == 0)
        pos = float2(-1.0, -1.0);
    else if (vertexID == 1)
        pos = float2(-1.0, 3.0);
    else
        pos = float2(3.0, -1.0);

    VSOutput output;
    output.ClipPos = float4(pos, 0.0, 1.0);
    return output;
}

float4 mainPS(VSOutput input) : SV_TARGET
{
    return OverlayColor;
}
