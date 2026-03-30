#ifndef VERTEX_LAYOUTS_HLSL
#define VERTEX_LAYOUTS_HLSL

// ============================================================
// VS Input Layouts — C++ VertexTypes.h 와 1:1 대응
// ============================================================

// FVertex (Position + Color)
// 사용: Primitive, Editor, Gizmo, Outline, Line
struct VS_Input_PC
{
    float3 position : POSITION;
    float4 color    : COLOR;
};

// FVertexPNCT (Position + Normal + Color + TexCoord)
// 사용: StaticMesh, OutlinePNCT
struct VS_Input_PNCT
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
    float2 texcoord : TEXTCOORD;
};

// FTextureVertex (Position + TexCoord)
// 사용: Font, SubUV, OverlayFont
struct VS_Input_PT
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
};

// Position only (Outline primitive expansion)
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
    float2 texcoord : TEXTCOORD;
};

// SV_POSITION only (Outline)
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

#endif // VERTEX_LAYOUTS_HLSL
