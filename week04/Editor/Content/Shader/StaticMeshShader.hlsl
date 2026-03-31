cbuffer FMeshUnlitConstants : register(b0)
{
    row_major matrix MVP;
    row_major matrix World;
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
    
    // 법선을 월드 공간으로 변환 (회전 반영)
    Output.Normal = normalize(mul(Input.Normal, (float3x3)World));
    
    Output.UV = Input.UV;
    return Output;
}

float4 PSMain(VS_OUTPUT Input) : SV_Target
{
    float4 TexColor = DiffuseTexture.Sample(LinearSampler, Input.UV);
    float3 N = normalize(Input.Normal);
    
    // 고정된 광원 방향 (월드 공간 기준)
    float3 L = normalize(float3(0.5f, 1.0f, 0.5f));
    
    // Lambertian Diffuse
    float Diffuse = max(dot(N, L), 0.0f);
    
    // Ambient
    float Ambient = 0.2f;
    
    float Lighting = Diffuse + Ambient;
    
    return float4(TexColor.rgb * BaseColor.rgb * Lighting, TexColor.a * BaseColor.a);
}