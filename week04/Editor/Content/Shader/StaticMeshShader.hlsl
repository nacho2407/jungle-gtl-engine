cbuffer FMeshLitConstants : register(b0)
{
    row_major matrix MVP;
    row_major matrix World;
    float4 BaseColor;
    uint bEnableLighting;
    float Time;
    float ScrollSpeedX;
    float ScrollSpeedY;
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
    
    // UV Scroll 적용
    Output.UV = Input.UV + float2(ScrollSpeedX, ScrollSpeedY) * Time;
    
    return Output;
}

float4 PSMain(VS_OUTPUT Input) : SV_Target
{
    float4 TexColor = DiffuseTexture.Sample(LinearSampler, Input.UV);
    float4 FinalColor = TexColor * BaseColor;

    if (bEnableLighting != 0)
    {
        float3 N = normalize(Input.Normal);
        float3 L = normalize(float3(0.5f, 1.0f, 0.5f));
        
        // 1. Half-Lambert (빛을 부드럽게 감싸 실내 가시성 확보)
        float Lambert = dot(N, L);
        float HalfLambert = Lambert * 0.5f + 0.5f;
        float Diffuse = HalfLambert * HalfLambert; // 제곱을 통해 더 예쁜 대비 생성
        
        // 2. Hemisphere Ambient (하늘/지면 대비를 통한 입체감)
        // 위를 보는 면은 0.3, 아래를 보는 면은 0.1 정도의 앰비언트
        float SkyFactor = N.y * 0.5f + 0.5f;
        float3 Ambient = lerp(float3(0.1f, 0.1f, 0.12f), float3(0.25f, 0.25f, 0.3f), SkyFactor);
        
        float3 Lighting = Diffuse * float3(1.0f, 1.0f, 0.95f) + Ambient;
        FinalColor.rgb *= Lighting;
    }
    
    return FinalColor;
}