Texture2D<uint> IdTexture : register(t0);

struct VSOutput
{
    float4 Position : SV_POSITION;
};

VSOutput VS(uint VertexId : SV_VertexID)
{
    VSOutput Output;
    float2 Position = VertexId == 0 ? float2(-1.0f, -1.0f) :
        (VertexId == 1 ? float2(-1.0f, 3.0f) : float2(3.0f, -1.0f));
    Output.Position = float4(Position, 0.0f, 1.0f);
    return Output;
}

float4 PS(VSOutput Input) : SV_TARGET
{
    uint Id = IdTexture.Load(int3(Input.Position.xy, 0));
    if (Id == 0)
    {
        return float4(0.02f, 0.02f, 0.025f, 1.0f);
    }

    uint Hash = Id * 1664525u + 1013904223u;
    float R = ((Hash >> 0) & 255u) / 255.0f;
    float G = ((Hash >> 8) & 255u) / 255.0f;
    float B = ((Hash >> 16) & 255u) / 255.0f;
    return float4(max(R, 0.18f), max(G, 0.18f), max(B, 0.18f), 1.0f);
}
