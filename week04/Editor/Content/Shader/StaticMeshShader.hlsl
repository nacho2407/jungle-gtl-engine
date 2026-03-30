// 상수 버퍼 (C++의 FMeshUnlitConstants와 메모리 레이아웃이 정확히 일치해야 함)
cbuffer FMeshUnlitConstants : register(b0)
{
    row_major matrix MVP; // 64 bytes (4x4 matrix)
    float4 BaseColor; // 16 bytes
};

// 텍스처 및 샘플러 (추후 머티리얼 연동 시 사용)
Texture2D DiffuseTexture : register(t0);
SamplerState LinearSampler : register(s0);

// 버텍스 셰이더 입력 (C++의 D3D11_INPUT_ELEMENT_DESC와 일치)
struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

// 픽셀 셰이더로 넘겨줄 데이터
struct VS_OUTPUT
{
    float4 Position : SV_POSITION; // 시스템 제공 클립 공간 좌표
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD;
};

// ==============================================================================
// 버텍스 셰이더 (Vertex Shader)
// ==============================================================================
VS_OUTPUT VSMain(VS_INPUT Input)
{
    VS_OUTPUT Output;
    
    // 로컬 좌표를 월드-뷰-투영(MVP) 공간으로 변환
    Output.Position = mul(float4(Input.Position, 1.0f), MVP);
    
    // 노멀과 UV는 그대로 픽셀 셰이더로 전달 
    // (조명 연산이 필요해지면 여기서 노멀을 월드 공간으로 변환해야 함)
    Output.Normal = Input.Normal;
    Output.UV = Input.UV;
    
    return Output;
}

// ==============================================================================
// 픽셀 셰이더 (Pixel Shader)
// ==============================================================================
float4 PSMain(VS_OUTPUT Input) : SV_Target
{
    // [1단계] 단색 렌더링 (현재 C++ 코드에서 넘겨주는 BaseColor만 사용)
    float4 FinalColor = BaseColor;
    
    // [2단계] 추후 머티리얼(텍스처) 연동이 완료되면 아래 주석을 풀어서 사용
    // float4 TexColor = DiffuseTexture.Sample(LinearSampler, Input.UV);
    // FinalColor = TexColor * BaseColor;

    // (선택) 조명 연산이 추가된다면 Input.Normal을 활용해 디렉셔널 라이팅 연산 추가 가능

    return FinalColor;
}