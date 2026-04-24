
/*
    DeferredOpaquePass.hlsl는 deferred 경로의 opaque 장면 패스 셰이더입니다.
*/

#include "../../Common/Surface/CommonTypes.hlsli"
#include "../../Common/Surface/SurfaceData.hlsli"
#include "../../Common/Lighting/LightingCommon.hlsli"
#include "DeferredOpaqueCommon.hlsli"

Texture2D g_txColor : register(t0);

#if defined(USE_NORMAL_MAP)
Texture2D g_NormalMap : register(t1);
#define DEFERRED_OPAQUE_NORMAL_TEX g_NormalMap
#else
#define DEFERRED_OPAQUE_NORMAL_TEX g_txColor
#endif

Texture2D g_SpecularMap : register(t2);

float4 PS_Opaque_Unlit(FOpaqueVSOutput Input) : SV_TARGET0
{
    return EncodeBaseColor(ResolveDeferredOpaqueColor(Input, g_txColor));
}

FOpaqueOutput2 PS_Opaque_Gouraud(FOpaqueVSOutput Input)
{
    FOpaqueOutput2 Output;
    Output.BaseColor = EncodeBaseColor(ResolveDeferredOpaqueColor(Input, g_txColor));
    Output.Surface1  = Input.gouraud;
    return Output;
}

FOpaqueOutput2 PS_Opaque_Lambert(FOpaqueVSOutput Input)
{
    FOpaqueOutput2 Output;
    Output.BaseColor = EncodeBaseColor(ResolveDeferredOpaqueColor(Input, g_txColor));
    Output.Surface1  = EncodeNormal(ResolveDeferredOpaqueNormal(Input, DEFERRED_OPAQUE_NORMAL_TEX));
    return Output;
}

FOpaqueOutput3 PS_Opaque_BlinnPhong(FOpaqueVSOutput Input)
{
    FOpaqueOutput3 Output;
    Output.BaseColor = EncodeBaseColor(ResolveDeferredOpaqueColor(Input, g_txColor));
    Output.Surface1  = EncodeNormal(ResolveDeferredOpaqueNormal(Input, DEFERRED_OPAQUE_NORMAL_TEX));

    float Shininess        = MaterialParam.x > 0.0f ? MaterialParam.x : 32.0f;
    float SpecularStrength = MaterialParam.y > 0.0f ? MaterialParam.y : 0.3f;
    if (StaticMeshHasSpecularTexture())
    {
        SpecularStrength *= g_SpecularMap.Sample(LinearWrapSampler, Input.texcoord).r;
    }

    Output.Surface2 = EncodeMaterialParam(float4(Shininess, SpecularStrength, 0.0f, 1.0f));
    return Output;
}

#undef DEFERRED_OPAQUE_NORMAL_TEX
