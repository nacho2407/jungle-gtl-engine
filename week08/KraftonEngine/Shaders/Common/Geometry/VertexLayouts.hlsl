#ifndef VERTEX_LAYOUTS_HLSL
#define VERTEX_LAYOUTS_HLSL

// ============================================================
// VS Input Layouts — C++ VertexTypes.h 와 1:1 대응
// ============================================================

// FVertex (Position + Color)
struct VS_Input_PC
{
    float3 position : POSITION;
    float4 color    : COLOR;
};

// FVertexPNCT (Position + Normal + Color + TexCoord)
struct VS_Input_PNCT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD0;
};

// FVertexPNCT_T (Position + Normal + Color + TexCoord + Tangent)
struct VS_Input_PNCT_T
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD0;
    float4 tangent  : TANGENT;
};

// FTextureVertex (Position + TexCoord)
struct VS_Input_PT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

// Position only
struct VS_Input_P
{
    float3 position : POSITION;
};

// ============================================================
// PS Input (VS -> PS 전달 구조체)
// ============================================================

// SV_POSITION + Color
struct PS_Input_Color
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
};

// SV_POSITION + TexCoord
struct PS_Input_Tex
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};

// SV_POSITION + Normal + Color + TexCoord (StaticMesh)
struct PS_Input_Full
{
    float4 position : SV_POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 texcoord : TEXCOORD;
};

// SV_POSITION + WorldPos + Normal + UV + Tangent + Color (Uber Shader)
struct PS_Input_PNCT_T
{
    float4 position  : SV_POSITION;
    float3 worldPos  : TEXCOORD0;
    float3 normal    : TEXCOORD1;
    float2 uv        : TEXCOORD2;
    float4 tangent   : TEXCOORD3;
    float4 color     : COLOR;
};

// SV_POSITION + WorldPos + Normal + Color (Decal)
struct PS_Input_Decal
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float4 color    : COLOR;
};

// SV_POSITION + UV (PostProcess)
struct PS_Input_UV
{
    float4 position : SV_POSITION;
    float2 uv       : TEXCOORD0;
};

// SV_POSITION only
struct PS_Input_PosOnly
{
    float4 position : SV_POSITION;
};

// SV_POSITION + Color + WorldPos (Editor)
struct PS_Input_ColorWorld
{
    float4 position : SV_POSITION;
    float4 color    : COLOR;
    float3 worldPos : TEXCOORD0;
};

// ============================================================
// PS Output (픽셀 쉐이더 출력 구조체)
// ============================================================

// G-Buffer Output (BaseDraw Pass)
struct PS_Output_GBuffer
{
    float4 BaseColor : SV_Target0;
    float4 Normal    : SV_Target1;
    float4 GouraudL  : SV_Target2; // Per-vertex light for Gouraud shading
};

#endif // VERTEX_LAYOUTS_HLSL
