//#include "Common/ConstantBuffers.hlsl"
//#include "Common/Functions.hlsl"
//struct VS_INPUT
//{
//    float3 Pos : POSITION;
//    // UV, Normal 등은 PreDepth에서 굳이 필요 없습니다. (Alpha Test가 없다면)
//};

//struct VS_OUTPUT
//{
//    float4 Pos : SV_POSITION;
//};

//// 1. Vertex Shader: 위치만 계산합니다.
//VS_OUTPUT VS_Main(VS_INPUT input)
//{
//    VS_OUTPUT output;
//    //output.Pos = ApplyMVP(input.Pos);
//    return output;
//}

//// 2. Pixel Shader: 불투명(Opaque) 오브젝트라면 아무것도 안 해도 됩니다.
//void PS_Main(VS_OUTPUT input)
//{
//    //Depth 스펙에 의해 Z값만 자동으로 기록됨
//}