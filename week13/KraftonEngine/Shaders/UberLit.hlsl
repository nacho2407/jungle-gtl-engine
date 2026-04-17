// =============================================================================
// UberLit.hlsl — Uber Shader for Forward Shading
// =============================================================================
// Preprocessor Definitions (C++ 에서 D3D_SHADER_MACRO 로 전달):
//   LIGHTING_MODEL_GOURAUD  1  — 정점 단계 라이팅 (Gouraud Shading)
//   LIGHTING_MODEL_LAMBERT  1  — 픽셀 단계 Diffuse only (Lambert)
//   LIGHTING_MODEL_PHONG    1  — 픽셀 단계 Diffuse + Specular (Blinn-Phong)
//   HAS_NORMAL_MAP          1  — Normal Map 사용 여부 (팀원 C 통합용)
//   DEBUG_LIGHTS            1  — CB/SB 없이 하드코딩 라이트로 테스트
//
// 아무 라이팅 모델 매크로도 없으면 기본값 = Blinn-Phong
// =============================================================================

#include "Common/Functions.hlsli"
#include "Common/VertexLayouts.hlsli"
#include "Common/SystemSamplers.hlsli"

// ── 기본값 설정 ──
#if !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG)
#define LIGHTING_MODEL_PHONG 1
#endif

#ifndef DEBUG_LIGHTS
#define DEBUG_LIGHTS 1
#endif

// ── 라이팅 구조체 & 리소스 바인딩 ──
#if !DEBUG_LIGHTS
// 실제 라이팅: CB/SB 바인딩 포함 전체 정의
#include "Common/ForwardLightData.hlsli"
#else
// 디버그 모드: FLightInfo 구조체만 필요 (CB/SB 바인딩 없음)
#define LIGHT_TYPE_POINT 0
#define LIGHT_TYPE_SPOT  1

struct FLightInfo
{
    float4 Color;
    float3 Position;      float Intensity;
    float  AttenuationRadius; float FalloffExponent; uint LightType; float _pad0;
    float3 Direction;     float InnerConeCos;
    float  OuterConeCos;  float3 _pad1;
};
#endif

// =============================================================================
// 텍스처
// =============================================================================
Texture2D g_txDiffuse : register(t0);

#if defined(HAS_NORMAL_MAP) && HAS_NORMAL_MAP
Texture2D g_txNormal  : register(t1);
#endif

// ── Per-Object Material (b2) — 기존 StaticMesh 와 레이아웃 동일 (호환성) ──
cbuffer PerShader1 : register(b2)
{
    float4 SectionColor;
};

// 머티리얼 확장 파라미터 — 팀원 A CB 시스템 완성 후 b2 확장 예정
static const float4 g_DefaultEmissive  = float4(0, 0, 0, 0);
static const float  g_DefaultShininess = 32.0f;

// =============================================================================
// VS ↔ PS 인터페이스
// =============================================================================
struct UberVS_Output
{
    float4 position  : SV_POSITION;
    float3 normal    : NORMAL;
    float4 color     : COLOR0;
    float2 texcoord  : TEXCOORD0;
    float3 worldPos  : TEXCOORD1;
#if defined(LIGHTING_MODEL_GOURAUD) && LIGHTING_MODEL_GOURAUD
    float3 litDiffuse  : TEXCOORD2;
    float3 litSpecular : TEXCOORD3;
#endif
};

// =============================================================================
// 라이팅 유틸 함수
// =============================================================================

float CalcAttenuation(float dist, float radius, float falloff)
{
    float ratio = saturate(dist / max(radius, 0.0001f));
    return pow(1.0f - ratio, falloff);
}

float3 CalcAmbient(float3 lightColor, float intensity)
{
    return lightColor * intensity;
}

float3 CalcDirectionalDiffuse(float3 lightColor, float3 lightDir, float intensity, float3 N)
{
    float NdotL = saturate(dot(N, -lightDir));
    return lightColor * intensity * NdotL;
}

float3 CalcDirectionalSpecular(float3 lightColor, float3 lightDir, float intensity,
                               float3 N, float3 V, float shininess)
{
    float3 H = normalize(-lightDir + V);
    float NdotH = saturate(dot(N, H));
    return lightColor * intensity * pow(NdotH, max(shininess, 1.0f));
}

// ── 통합 FLightInfo 기반 계산 (Point/Spot 공용) ──
float3 CalcLightDiffuse(FLightInfo light, float3 worldPos, float3 N)
{
    float3 L    = light.Position - worldPos;
    float  dist = length(L);
    L = normalize(L);

    float atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);
    float NdotL = saturate(dot(N, L));

    // Spot cone falloff (Point는 InnerConeCos=-1, OuterConeCos=-1 → spotFactor=1)
    float spotFactor = 1.0f;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        float cosAngle = dot(-L, normalize(light.Direction));
        spotFactor = smoothstep(light.OuterConeCos, light.InnerConeCos, cosAngle);
    }

    return light.Color.rgb * light.Intensity * NdotL * atten * spotFactor;
}

float3 CalcLightSpecular(FLightInfo light, float3 worldPos, float3 N, float3 V, float shininess)
{
    float3 L    = normalize(light.Position - worldPos);
    float  dist = length(light.Position - worldPos);
    float  atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);

    float spotFactor = 1.0f;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        float cosAngle = dot(-L, normalize(light.Direction));
        spotFactor = smoothstep(light.OuterConeCos, light.InnerConeCos, cosAngle);
    }

    float3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    return light.Color.rgb * light.Intensity * pow(NdotH, max(shininess, 1.0f)) * atten * spotFactor;
}

// =============================================================================
// 디버그 라이트 (CB/SB 없이 즉시 확인용)
// =============================================================================
#if DEBUG_LIGHTS
static const float3 g_DbgAmbientColor  = float3(0.15f, 0.15f, 0.18f);
static const float3 g_DbgDirLightDir   = normalize(float3(1.0f, -1.0f, 0.5f));
static const float3 g_DbgDirLightColor = float3(1.0f, 0.95f, 0.85f);
static const float  g_DbgDirIntensity  = 1.0f;

// FLightInfo 디버그 인스턴스 (Point 2개)
static const FLightInfo g_DbgLights[2] =
{
    // Point Light 0 (따뜻한 주황)
    {
        float4(1.0f, 0.4f, 0.2f, 1.0f),   // Color
        float3(3.0f, 3.0f, 0.0f), 1.0f,    // Position, Intensity
        10.0f, 2.0f, LIGHT_TYPE_POINT, 0,   // Radius, Falloff, Type, _pad
        float3(0, 0, 0), -1.0f,            // Direction, InnerConeCos (unused)
        -1.0f, float3(0, 0, 0)             // OuterConeCos, _pad (unused)
    },
    // Point Light 1 (차가운 파랑)
    {
        float4(0.2f, 0.5f, 1.0f, 1.0f),
        float3(-3.0f, 2.0f, -2.0f), 1.0f,
        8.0f, 2.0f, LIGHT_TYPE_POINT, 0,
        float3(0, 0, 0), -1.0f,
        -1.0f, float3(0, 0, 0)
    },
};
static const uint g_DbgLightCount = 2;
#endif

// =============================================================================
// 통합 라이팅 누적
// =============================================================================

float3 AccumulateDiffuse(float3 worldPos, float3 N)
{
    float3 result = float3(0, 0, 0);

#if DEBUG_LIGHTS
    result += g_DbgAmbientColor;
    result += CalcDirectionalDiffuse(g_DbgDirLightColor, g_DbgDirLightDir, g_DbgDirIntensity, N);
    for (uint i = 0; i < g_DbgLightCount; ++i)
        result += CalcLightDiffuse(g_DbgLights[i], worldPos, N);
#else
    // Ambient + Directional (CB b3)
    result += CalcAmbient(AmbientLight.Color.rgb, AmbientLight.Intensity);
    result += CalcDirectionalDiffuse(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                     DirectionalLight.Intensity, N);

    // Point/Spot (StructuredBuffer t8, Tile Culling 적용 시 t9/t10 경유)
    #if defined(USE_TILE_CULLING) && USE_TILE_CULLING
    // ── Tile-based: 이 픽셀이 속한 타일의 라이트만 순회 ──
    uint2 tileCoord = uint2(worldPos.xy); // PS에서는 SV_Position 기반으로 교체 필요
    uint tileIdx    = tileCoord.y * NumTilesX + tileCoord.x;
    uint2 gridData  = g_TileLightGrid[tileIdx];
    for (uint t = 0; t < gridData.y; ++t)
    {
        uint lightIdx = g_TileLightIndices[gridData.x + t];
        result += CalcLightDiffuse(g_AllLights[lightIdx], worldPos, N);
    }
    #else
    // ── 전수 순회 (Culling 미적용) ──
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        result += CalcLightDiffuse(g_AllLights[i], worldPos, N);
    #endif
#endif

    return result;
}

float3 AccumulateSpecular(float3 worldPos, float3 N, float3 V, float shininess)
{
    float3 result = float3(0, 0, 0);

#if DEBUG_LIGHTS
    result += CalcDirectionalSpecular(g_DbgDirLightColor, g_DbgDirLightDir, g_DbgDirIntensity, N, V, shininess);
    for (uint i = 0; i < g_DbgLightCount; ++i)
        result += CalcLightSpecular(g_DbgLights[i], worldPos, N, V, shininess);
#else
    result += CalcDirectionalSpecular(DirectionalLight.Color.rgb, DirectionalLight.Direction,
                                      DirectionalLight.Intensity, N, V, shininess);

    #if defined(USE_TILE_CULLING) && USE_TILE_CULLING
    uint2 tileCoord = uint2(worldPos.xy);
    uint tileIdx    = tileCoord.y * NumTilesX + tileCoord.x;
    uint2 gridData  = g_TileLightGrid[tileIdx];
    for (uint t = 0; t < gridData.y; ++t)
    {
        uint lightIdx = g_TileLightIndices[gridData.x + t];
        result += CalcLightSpecular(g_AllLights[lightIdx], worldPos, N, V, shininess);
    }
    #else
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        result += CalcLightSpecular(g_AllLights[i], worldPos, N, V, shininess);
    #endif
#endif

    return result;
}

// =============================================================================
// Vertex Shader
// =============================================================================
UberVS_Output VS(VS_Input_PNCT input)
{
    UberVS_Output output;

    float4 worldPos4 = mul(float4(input.position, 1.0f), Model);
    output.worldPos  = worldPos4.xyz;
    output.position  = mul(mul(worldPos4, View), Projection);
    output.normal    = normalize(mul(input.normal, (float3x3)Model));
    output.color     = input.color * SectionColor;
    output.texcoord  = input.texcoord;

#if defined(LIGHTING_MODEL_GOURAUD) && LIGHTING_MODEL_GOURAUD
    float3 N = output.normal;
    float3 V = normalize(CameraWorldPos - output.worldPos);
    output.litDiffuse  = AccumulateDiffuse(output.worldPos, N);
    output.litSpecular = AccumulateSpecular(output.worldPos, N, V, g_DefaultShininess);
#endif

    return output;
}

// =============================================================================
// Pixel Shader
// =============================================================================
float4 PS(UberVS_Output input) : SV_TARGET
{
    float4 texColor = g_txDiffuse.Sample(LinearWrapSampler, input.texcoord);
    if (texColor.a < 0.001f)
        texColor = float4(1.0f, 1.0f, 1.0f, 1.0f);

    float4 baseColor = texColor * input.color;

    float3 N = normalize(input.normal);

#if defined(HAS_NORMAL_MAP) && HAS_NORMAL_MAP
    // TODO: TBN 행렬 연동 (팀원 C)
    // float3 sampledN = g_txNormal.Sample(LinearWrapSampler, input.texcoord).rgb * 2.0 - 1.0;
    // N = normalize(mul(sampledN, TBN));
#endif

    float3 V = normalize(CameraWorldPos - input.worldPos);

    float3 diffuse  = float3(0, 0, 0);
    float3 specular = float3(0, 0, 0);

#if defined(LIGHTING_MODEL_GOURAUD) && LIGHTING_MODEL_GOURAUD
    // Gouraud: VS에서 정점 단위로 계산 → PS에서 보간된 값 사용
    diffuse  = input.litDiffuse;
    specular = input.litSpecular;

#elif defined(LIGHTING_MODEL_LAMBERT) && LIGHTING_MODEL_LAMBERT
    diffuse = AccumulateDiffuse(input.worldPos, N);

#elif defined(LIGHTING_MODEL_PHONG) && LIGHTING_MODEL_PHONG
    diffuse  = AccumulateDiffuse(input.worldPos, N);
    specular = AccumulateSpecular(input.worldPos, N, V, g_DefaultShininess);
#endif

    // Diffuse에만 albedo를 곱하고, Specular는 빛 색상 그대로 더한다
    // (비금속 표면: specular 반사 = 빛의 색, 물체 색이 아님)
    float3 finalColor = baseColor.rgb * diffuse + specular + g_DefaultEmissive.rgb;
    finalColor = ApplyWireframe(finalColor);

    return float4(finalColor, baseColor.a);
}
