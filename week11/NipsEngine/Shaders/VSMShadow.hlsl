
Texture2D ShadowMap : register(t10);
SamplerState PointSampler : register(s2);
// samplerstate 추가해야함
struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VS_OUTPUT VSMShadowVS(uint VertexID : SV_VertexID)
{
    VS_OUTPUT Output;
    
    // VertexID 0 -> uv(0, 0), pos(-1,  1)
    // VertexID 1 -> uv(2, 0), pos( 3,  1)
    // VertexID 2 -> uv(0, 2), pos(-1, -3)
    Output.TexCoord = float2((VertexID << 1) & 2, VertexID & 2);
    
    // NDC 좌표계로 변환 (0~2 범위를 -1~3 범위로 매핑)
    Output.Position = float4(Output.TexCoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return Output;
}
// pixelshader가 float2를 반환하면 어떻게 되는가?
float2 VSMShadowPS(VS_OUTPUT input) : SV_Target
{   
    float depth = ShadowMap.Sample(PointSampler, input.TexCoord).r;
    return float2(depth, depth * depth);

}