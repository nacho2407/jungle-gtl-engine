#include "Common/Functions.hlsl"
#include "Common/VertexLayouts.hlsl"
#include "Common/ConstantBuffers.hlsl"

Texture2D g_txColor : register(t0);
SamplerState g_Sample : register(s0);

PS_Input_Full VS(VS_Input_PNCT input)
{
    PS_Input_Full output;
    output.position = ApplyMVP(input.position); 
    return output;
}

void PS()
{

}
