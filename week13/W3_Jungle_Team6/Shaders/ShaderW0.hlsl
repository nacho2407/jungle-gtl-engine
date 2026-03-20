/* Constant Buffers */

cbuffer TransformBuffer : register(b0)
{
    row_major float4x4 Model;
    row_major float4x4 View;
    row_major float4x4 Projection;
};

cbuffer GizmoBuffer : register(b1)
{
    float4 GizmoColorTint;
    uint bIsInnerGizmo;
    uint bClicking;
    uint SelectedAxis;
    float HoveredAxisOpacity;
};


cbuffer OverlayBuffer : register(b2)
{
    float2 OverlayCenterScreen;
    float2 ViewportSize;

    float OverlayRadius;
    float3 Padding2;

    float4 OverlayColor;
};

//  Axis : 0, Grid : 1 (CamearPosition과 곱하면 Axis는 CameraPosition에 영향을 받지 않음)
cbuffer EditorBuffer : register(b3)
{
    float4 CameraPosition;
    uint EditorFlag;
    float3 Padding3;
};

cbuffer OutlineConstants : register(b4)
{
    float4 OutlineColor;
    float3 OutlineInvScale;
    float OutlineOffset;
    uint PrimitiveType; //  0 : 2D, 1 : 3D
    float3 Padding4;
};

/* Common VS Helper */

float4 ApplyMVP(float3 pos)
{
    float4 world = mul(float4(pos, 1.0f), Model);
    float4 view = mul(world, View);
    return mul(view, Projection);
}

/* Primitive */

struct PrimitiveVSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct PrimitivePSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

PrimitivePSInput PrimitiveVS(PrimitiveVSInput input)
{
    PrimitivePSInput output;
    output.position = ApplyMVP(input.position);
    output.color = input.color;
    return output;
}

float4 PrimitivePS(PrimitivePSInput input) : SV_TARGET
{
    return input.color;
}

/* Gizmo */

uint GetAxisFromColor(float3 color)
{
    if (color.g >= color.r && color.g >= color.b)
        return 1;
    if (color.b >= color.r && color.b >= color.g)
        return 2;
    return 0;
}

PrimitivePSInput GizmoVS(PrimitiveVSInput input)
{
    PrimitivePSInput output;
    output.position = ApplyMVP(input.position);
    output.color = input.color * GizmoColorTint;
    return output;
}

float4 GizmoPS(PrimitivePSInput input) : SV_TARGET
{
    uint axis = GetAxisFromColor(input.color.rgb);
    float4 outColor = input.color;

    if (axis == SelectedAxis)
    {
        outColor.rgb = float3(1, 1, 0);
        outColor.a = 1.0f;
    }
    
    if ((bool) bIsInnerGizmo)
    {
        outColor.a *= HoveredAxisOpacity;
    }
    
    if (axis != SelectedAxis && bClicking)
    {
        discard;
    }

    return saturate(outColor);
}

/* Overlay */

struct OverlayVSInput
{
    float3 position : POSITION;
};

struct OverlayPSInput
{
    float4 position : SV_POSITION;
    float2 localPos : TEXCOORD0;
};

OverlayPSInput OverlayVS(OverlayVSInput input)
{
    OverlayPSInput output;

    float2 localPos = float2(input.position.x, input.position.y) * 2.0f;
    float2 pixelOffset = localPos * OverlayRadius;
    float2 screenPos = OverlayCenterScreen + pixelOffset;

    float2 ndc;
    ndc.x = (screenPos.x / ViewportSize.x) * 2.0f - 1.0f;
    ndc.y = 1.0f - (screenPos.y / ViewportSize.y) * 2.0f;

    output.position = float4(ndc, 0, 1);
    output.localPos = localPos;

    return output;
}

float4 OverlayPS(OverlayPSInput input) : SV_TARGET
{
    float dist = length(input.localPos);

    if (dist > 1.0f)
        discard;

    return OverlayColor;
}


/* Editor */
struct EditorVSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct EditorPSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 worldPos : TEXCOORD0;
};

EditorPSInput EditorVS(EditorVSInput input)
{
    EditorPSInput output;
    
    if (!(bool)EditorFlag)
    {
        float3 axisDir = input.position.xyz;
        float InfinityLength = 100000.0f;

        float3 worldPos = float3(0.0f, 0.0f, 0.0f);

        if (abs(axisDir.x) > 0.0f)
            worldPos.x = CameraPosition.x + (axisDir.x * InfinityLength);
        if (abs(axisDir.y) > 0.0f)
            worldPos.y = CameraPosition.y + (axisDir.y * InfinityLength);
        if (abs(axisDir.z) > 0.0f)
            worldPos.z = CameraPosition.z + (axisDir.z * InfinityLength);

        output.worldPos = worldPos;

        float4 viewPos = mul(float4(worldPos, 1.0f), View);
        output.position = mul(viewPos, Projection);
    
        output.color = input.color;
        return output;
    }
	else
	{
        float3 scaledPos = input.position.xyz * 100000.0f;
        float3 worldPos = float3(CameraPosition.x + scaledPos.x, CameraPosition.y + scaledPos.y, 0.0f);
		
        output.worldPos = worldPos;
		
        float4 viewPos = mul(float4(worldPos, 1.0f), View);
        output.position = mul(viewPos, Projection);
		
        output.color = input.color;
		
        return output;
    }
}

float4 EditorPS(EditorPSInput input) : SV_TARGET
{
    if (EditorFlag == 0)
    {
        return input.color;
    }
    else
    {
        float MinorThickness = 0.8f;
        float MajorThickness = 1.2f;
        
        float2 coord1 = input.worldPos.xy / 10.0f;
        float2 deriv1 = fwidth(coord1);
        float2 grid1 = abs(frac(coord1 - 0.5f) - 0.5f);
        
        float2 lineAlpha1 = smoothstep(deriv1 * MinorThickness, deriv1 * (MinorThickness * 0.2f), grid1);
        float minorAlpha = max(lineAlpha1.x, lineAlpha1.y);
        
        float2 coord2 = input.worldPos.xy / 100.0f;
        float2 deriv2 = fwidth(coord2);
        float2 grid2 = abs(frac(coord2 - 0.5f) - 0.5f);
        
        float2 lineAlpha2 = smoothstep(deriv2 * MajorThickness, deriv2 * (MajorThickness * 0.2f), grid2);
        float majorAlpha = max(lineAlpha2.x, lineAlpha2.y);
        
        float dist = distance(CameraPosition.xyz, input.worldPos);
        float fade = 1.0f - saturate(dist / 2000.0f);

        float2 originDist = abs(input.worldPos.xy);
        float2 originDeriv = fwidth(input.worldPos.xy);

        float AxisThickness = MajorThickness * 1.5f;

        float2 axisAlpha = 1.0f - smoothstep(originDeriv * (AxisThickness * 0.2f), originDeriv * AxisThickness, originDist);

        float3 gridColor = float3(0.6f, 0.6f, 0.6f);
        float finalAlpha = max(minorAlpha * 0.3f, majorAlpha * 1.0f);

        if (axisAlpha.y > 0.0f) finalAlpha = 0.0f;
        if (axisAlpha.x > 0.0f) finalAlpha = 0.0f;
        
        return float4(gridColor, finalAlpha * fade);
    }
}

/* Outline */
PrimitivePSInput OutlineVS(PrimitiveVSInput input)
{
    PrimitivePSInput output;
    
    float3 scaledLocalPos;

    if((bool)PrimitiveType) // 3D
    {
        float3 signDir = float3(
        input.position.x < 0.0f ? -1.0f : 1.0f,
        input.position.y < 0.0f ? -1.0f : 1.0f,
        input.position.z < 0.0f ? -1.0f : 1.0f
        );

        float3 localOffset = signDir * (OutlineOffset * OutlineInvScale);
        scaledLocalPos = input.position + localOffset;
    }
    else // 2D
    {
        //float2 signDir = float2(
        //input.position.x < 0.0f ? -1.0f : 1.0f,
        //input.position.y < 0.0f ? -1.0f : 1.0f
        //);
        
        //float2 localOffset = signDir * (OutlineOffset * OutlineInvScale.xy);
        //scaledLocalPos = input.position + float3(localOffset, 0.0f);
        scaledLocalPos = input.position * (1.0f + OutlineOffset);
    }

    float4 worldPos = mul(float4(scaledLocalPos, 1.0f), Model);
    float4 viewPos = mul(worldPos, View);
    output.position = mul(viewPos, Projection);

    return output;
}

float4 OutlinePS(PrimitivePSInput input) : SV_TARGET
{
    return OutlineColor;
}