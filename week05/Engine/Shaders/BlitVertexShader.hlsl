struct VSOutput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

static const float2 QuadPos[6] =
{
    float2(-1.0,  1.0),
    float2( 1.0,  1.0),
    float2(-1.0, -1.0),
    float2( 1.0,  1.0),
    float2( 1.0, -1.0),
    float2(-1.0, -1.0),
};

static const float2 QuadUV[6] =
{
    float2(0.0, 0.0),
    float2(1.0, 0.0),
    float2(0.0, 1.0),
    float2(1.0, 0.0),
    float2(1.0, 1.0),
    float2(0.0, 1.0),
};

VSOutput main(uint VertexID : SV_VertexID)
{
    VSOutput Output;
    Output.Position = float4(QuadPos[VertexID], 0.0, 1.0);
    Output.UV       = QuadUV[VertexID];
    return Output;
}
