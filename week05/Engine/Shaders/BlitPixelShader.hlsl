Texture2D    SceneTexture : register(t0);
SamplerState SceneSampler : register(s0);

struct PSInput
{
    float4 Position : SV_Position;
    float2 UV       : TEXCOORD0;
};

float4 main(PSInput Input) : SV_Target
{
    return SceneTexture.Sample(SceneSampler, Input.UV);
}
