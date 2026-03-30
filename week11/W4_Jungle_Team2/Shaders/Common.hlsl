/* Constant Buffers */


cbuffer FrameBuffer : register(b0)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
    float bIsWireframe;
    float3 WireframeRGB;
}

cbuffer PerObjectBuffer : register(b1)
{
    row_major float4x4 Model;
    float4 PrimitiveColor; 
};

cbuffer GizmoBuffer : register(b2)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
};


cbuffer OverlayBuffer : register(b3)
{
    float2 OverlayCenterScreen;
    float2 ViewportSize;

    float OverlayRadius;
    float3 Padding2;

    float4 OverlayColor;
};

cbuffer EditorBuffer : register(b4)
{
    float4 CameraPosition;
    uint EditorFlag;
    float3 Padding3;
};

cbuffer OutlineConstants : register(b5)
{
    float4 OutlineColor;
    float OutlineThicknessPixels;
    float2 OutlineViewportSize;
    float OutlinePadding0;
};

float4 ApplyMVP(float3 pos)
{
    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);
}
