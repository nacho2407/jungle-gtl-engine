
/*
    StaticMeshMaterialCommon.hlsli는 머티리얼 샘플링과 파라미터 해석을 제공합니다.

    바인딩 컨벤션
    - b0: Frame 상수 버퍼
    - b1: PerObject/Material 상수 버퍼
    - b2: Pass/Shader 상수 버퍼
    - b3: Material 또는 보조 상수 버퍼
    - b4: Light 상수 버퍼
    - t0~t5: 패스/머티리얼 SRV
    - t6: LocalLights structured buffer
    - t10: SceneDepth, t11: SceneColor, t13: Stencil
    - s0: LinearClamp, s1: LinearWrap, s2: PointClamp
    - u#: Compute/후처리용 UAV
    - 이 파일에서 직접 선언한 슬롯: b2
*/

#ifndef STATIC_MESH_MATERIAL_COMMON_HLSLI
#define STATIC_MESH_MATERIAL_COMMON_HLSLI

cbuffer StaticMeshMaterialBuffer : register(b2)
{
    float4 SectionColor;
    float4 MaterialParam;
    uint HasBaseTexture;
    uint HasNormalTexture;
    uint HasSpecularTexture;
    float StaticMeshMaterialPadding;
}

float4 GetStaticMeshSectionColorOrWhite()
{
    float Magnitude = abs(SectionColor.x) + abs(SectionColor.y) + abs(SectionColor.z) + abs(SectionColor.w);
    return (Magnitude < 0.0001f) ? float4(1.0f, 1.0f, 1.0f, 1.0f) : SectionColor;
}

float4 SampleStaticMeshBaseColor(Texture2D TextureRef, float2 UV)
{
    if (HasBaseTexture == 0)
    {
        return float4(1.0f, 1.0f, 1.0f, 1.0f);
    }

    return TextureRef.Sample(LinearWrapSampler, UV);
}

bool StaticMeshHasNormalTexture()
{
    return HasNormalTexture != 0;
}

bool StaticMeshHasSpecularTexture()
{
    return HasSpecularTexture != 0;
}

#endif

