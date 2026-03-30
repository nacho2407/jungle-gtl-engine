cbuffer FMeshUnlitConstants : register(b0)
{
    row_major matrix MVP;
    float4 BaseColor;
};

Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;
    Output.Position = mul(float4(Input.Position, 1.0f), MVP);
    Output.Normal = Input.Normal;
    Output.UV = Input.UV;
    return Output;
}

float4 PSMain(VS_OUTPUT Input) : SV_Target
{
    float4 TexColor = DiffuseTexture.Sample(LinearSampler, Input.UV);
    return TexColor * BaseColor;
}