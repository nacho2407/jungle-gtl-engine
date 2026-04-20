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
#if !defined(LIGHTING_MODEL_GOURAUD) && !defined(LIGHTING_MODEL_LAMBERT) && !defined(LIGHTING_MODEL_PHONG) && !defined(LIGHTING_MODEL_TOON)
#define LIGHTING_MODEL_PHONG 1
#endif

// 실제 라이팅: CB/SB 바인딩 포함 전체 정의
#include "Common/ForwardLightData.hlsli"

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
static const float4 g_DefaultEmissive = float4(0, 0, 0, 0);
static const float g_DefaultShininess = 32.0f;

// =============================================================================
// VS ↔ PS 인터페이스
// =============================================================================
struct UberVS_Output
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
    float3 worldPos : TEXCOORD1;
#if defined(LIGHTING_MODEL_GOURAUD) && LIGHTING_MODEL_GOURAUD
    float3 litDiffuse  : TEXCOORD2;
    float3 litSpecular : TEXCOORD3;
#endif
};

// =============================================================================
// Cluster HeatMap 유틸
// =============================================================================
float3 HeatColor(float t)
{
    t = saturate(t);
    float3 cold   = float3(0.02f, 0.05f, 0.15f);
    float3 blue   = float3(0.0f,  0.25f, 1.0f);
    float3 cyan   = float3(0.0f,  0.9f,  1.0f);
    float3 yellow = float3(1.0f,  0.95f, 0.0f);
    float3 red    = float3(1.0f,  0.05f, 0.0f);
    if (t < 0.25f) return lerp(cold,   blue,   t / 0.25f);
    if (t < 0.50f) return lerp(blue,   cyan,   (t - 0.25f) / 0.25f);
    if (t < 0.75f) return lerp(cyan,   yellow, (t - 0.50f) / 0.25f);
    return             lerp(yellow, red,    (t - 0.75f) / 0.25f);
}

// =============================================================================
// Culling Utility
// =============================================================================
uint DepthToSlice(float viewDepth)
{
    float safeDepth = clamp(viewDepth, CullState.NearZ, CullState.FarZ);
    float logDepth = log(safeDepth / CullState.NearZ) / log(CullState.FarZ / CullState.NearZ);
    return min((uint) floor(logDepth * CullState.ClusterZ), CullState.ClusterZ - 1);
}

uint ComputeClusterIndex(float4 PixelPos, float3 WorldPos)
{
    float4 World4 = float4(WorldPos, 1.f);
    float4 ViewPos = mul(World4, View);
    uint TileX = min((uint) (PixelPos.x / CullState.ScreenWidth * CullState.ClusterX), CullState.ClusterX - 1);
    uint TileY = min((uint) (PixelPos.y / CullState.ScreenHeight * CullState.ClusterY), CullState.ClusterY - 1);
    uint SliceZ = DepthToSlice(abs(ViewPos.z));
    
    uint ClusterIdx = SliceZ * CullState.ClusterX * CullState.ClusterY
    + TileY * CullState.ClusterX
    + TileX;
    
    return ClusterIdx;
}


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
    float3 L = light.Position - worldPos;
    float dist = length(L);
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
    float3 L = normalize(light.Position - worldPos);
    float dist = length(light.Position - worldPos);
    float atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);

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
//----------------TOON
static const float g_ToonSteps = 4.0f; // diffuse 단계 수
static const float g_ToonDarknessFloor = 0.25f; // 그림자 최소 밝기

static const float g_ToonShadowThreshold = 0.45f; // 이 값 아래면 어두운 밴드
static const float g_ToonShadowSoftness = 0.02f; // 0에 가까울수록 딱 끊김

static const float g_ToonSpecThreshold = 0.75f; // 높을수록 specular가 좁아짐
static const float g_ToonSpecStrength = 0.35f;

static const float g_ToonRimMin = 0.55f;
static const float g_ToonRimMax = 0.85f;
static const float g_ToonRimStrength = 0.25f;

float ToonStep(float NdotL)
{
    float x = saturate(NdotL); //범위 0~1
    float stepped = floor(x * g_ToonSteps); //범위를 0~4로
    stepped /= max(g_ToonSteps - 1.0f, 1.0f); //3으로 나눠서 0~1.xx 이지만 1으로 제한
    stepped = saturate(stepped);
    return lerp(g_ToonDarknessFloor, 1.0f, stepped); //darkfloor와 1.0사이를 선형보간
}

float3 CalcToonDirectionalDiffuse(float3 N)
{
    float NdotL = saturate(dot(N, -DirectionalLight.Direction));
    float band = ToonStep(NdotL); //NDotL을 계단으로
    return DirectionalLight.Color.rgb * DirectionalLight.Intensity * band;
}

float3 CalcToonPointSpotDiffuse(FLightInfo light, float3 worldPos, float3 N)
{
    float3 L = light.Position - worldPos;
    float dist = length(L);
    L = normalize(L);

    float atten = CalcAttenuation(dist, light.AttenuationRadius, light.FalloffExponent);
    float band = ToonStep(saturate(dot(N, L)));

    float spotFactor = 1.0f;
    if (light.LightType == LIGHT_TYPE_SPOT)
    {
        float cosAngle = dot(-L, normalize(light.Direction));
        spotFactor = smoothstep(light.OuterConeCos, light.InnerConeCos, cosAngle);
    }

    return light.Color.rgb * light.Intensity * atten * spotFactor * band;
}

float3 AccumulateToonDiffuse(float3 worldPos, float4 position, float3 N)
{
    float3 result = CalcAmbient(AmbientLight.Color.rgb, AmbientLight.Intensity) * 0.15;
    result += CalcToonDirectionalDiffuse(N);

    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
    {
        result += CalcToonPointSpotDiffuse(g_AllLights[i], worldPos, N);
    }

    return result;
}

float3 CalcToonSpecular(float3 N, float3 V, float3 lightDir, float3 lightColor, float intensity)
{
    float3 H = normalize(-lightDir + V);
    float NdotH = saturate(dot(N, H));
    float NdotL = saturate(dot(N, -lightDir));
    float spec = step(g_ToonSpecThreshold, NdotH) * step(0.05f, NdotL);
    return lightColor * intensity * spec;
}

float CalcRimMask(float3 N, float3 V)
{
    float rimDot = 1.0f - saturate(dot(N, V));
    return smoothstep(g_ToonRimMin, g_ToonRimMax, rimDot);
}


// =============================================================================
// 통합 라이팅 누적
// =============================================================================

float3 AccumulateDiffuse(float3 worldPos, float4 position, float3 N)
{
    float3 result = float3(0, 0, 0);
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
#elif defined(USE_CLUSTER_CULLING) && USE_CLUSTER_CULLING
    uint ClusterIdx = ComputeClusterIndex(position,worldPos);
    uint2 LightGridEntry = g_ClusterLightGrid[ClusterIdx];
    uint Offset = LightGridEntry.x;
    uint LightCount = LightGridEntry.y;
    for (uint t = 0; t < LightCount; ++t)
    {
        uint lightIdx = g_ClusterLightIndices[Offset + t];
        result += CalcLightDiffuse(g_AllLights[lightIdx], worldPos, N);
    }
#else
    // ── 전수 순회 (Culling 미적용) ──
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        result += CalcLightDiffuse(g_AllLights[i], worldPos, N);
#endif

    return result;
}

float3 AccumulateSpecular(float3 worldPos, float4 position, float3 N, float3 V, float shininess)
{
    float3 result = float3(0, 0, 0);


#if defined(USE_TILE_CULLING) && USE_TILE_CULLING
    uint2 tileCoord = uint2(worldPos.xy);
    uint tileIdx    = tileCoord.y * NumTilesX + tileCoord.x;
    uint2 gridData  = g_TileLightGrid[tileIdx];
    for (uint t = 0; t < gridData.y; ++t)
    {
        uint lightIdx = g_TileLightIndices[gridData.x + t];
        result += CalcLightSpecular(g_AllLights[lightIdx], worldPos, N, V, shininess);
    }
#elif defined(USE_CLUSTER_CULLING) && USE_CLUSTER_CULLING
    uint ClusterIdx = ComputeClusterIndex(position, worldPos);
    uint2 LightGridEntry = g_ClusterLightGrid[ClusterIdx];
    uint Offset = LightGridEntry.x;
    uint LightCount = LightGridEntry.y;
    for (uint t = 0; t < LightCount; ++t)
    {
        uint lightIdx = g_ClusterLightIndices[Offset + t];
        result += CalcLightSpecular(g_AllLights[lightIdx], worldPos, N, V, shininess);
    }
#else
    for (uint i = 0; i < NumActivePointLights + NumActiveSpotLights; ++i)
        result += CalcLightSpecular(g_AllLights[i], worldPos, N, V, shininess);
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
    output.worldPos = worldPos4.xyz;
    output.position = mul(mul(worldPos4, View), Projection);
    output.normal = normalize(mul(input.normal, (float3x3) Model));
    output.color = input.color * SectionColor;
    output.texcoord = input.texcoord;

#if defined(LIGHTING_MODEL_GOURAUD) && LIGHTING_MODEL_GOURAUD
    float3 N = output.normal;
    float3 V = normalize(CameraWorldPos - output.worldPos);
    output.litDiffuse  = AccumulateDiffuse(output.worldPos,output.position, N);
    output.litSpecular = AccumulateSpecular(output.worldPos,output.position, N, V, g_DefaultShininess);
#endif

    return output;
}

// =============================================================================
// MRT 출력 구조체
// =============================================================================
struct UberPS_Output
{
    float4 Color : SV_TARGET0; // 최종 색상 (기존 프레임 버퍼)
    float4 Normal : SV_TARGET1; // World Normal (GBuffer Normal RT)
};

// =============================================================================
// Pixel Shader
// =============================================================================
UberPS_Output PS(UberVS_Output input)
{
    UberPS_Output output;

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

    float3 diffuse = float3(0, 0, 0);
    float3 specular = float3(0, 0, 0);

#if defined(LIGHTING_MODEL_GOURAUD) && LIGHTING_MODEL_GOURAUD
    // Gouraud: VS에서 정점 단위로 계산 → PS에서 보간된 값 사용
    diffuse  = input.litDiffuse;
    specular = input.litSpecular;

#elif defined(LIGHTING_MODEL_LAMBERT) && LIGHTING_MODEL_LAMBERT
    diffuse = AccumulateDiffuse(input.worldPos,input.position, N);

#elif defined(LIGHTING_MODEL_PHONG) && LIGHTING_MODEL_PHONG
    diffuse = AccumulateDiffuse(input.worldPos, input.position, N);
    specular = AccumulateSpecular(input.worldPos, input.position, N, V, g_DefaultShininess);
    
#elif defined(LIGHTING_MODEL_TOON) && LIGHTING_MODEL_TOON
    diffuse = AccumulateToonDiffuse(input.worldPos,input.position, N);
#endif

    // Diffuse에만 albedo를 곱하고, Specular는 빛 색상 그대로 더한다
    // (비금속 표면: specular 반사 = 빛의 색, 물체 색이 아님)
    float3 finalColor = baseColor.rgb * diffuse + specular + g_DefaultEmissive.rgb;
#if defined(LIGHTING_MODEL_TOON) && LIGHTING_MODEL_TOON
    float rimMask = CalcRimMask(N,V);
    float3 rimColor = baseColor.rgb * rimMask * g_ToonRimStrength;
    finalColor += rimColor;
#endif
    finalColor = ApplyWireframe(finalColor);

#if defined(USE_CLUSTER_CULLING) && USE_CLUSTER_CULLING
    if (ShowClusterHeatMap)
    {
        uint ClusterIdx = ComputeClusterIndex(input.position, input.worldPos);
        uint LightCount = g_ClusterLightGrid[ClusterIdx].y;
        float t = (float) LightCount / max(MaxHeatLightCount, 1.0f);
        finalColor = lerp(finalColor, HeatColor(t), 0.65f);
    }
#endif

    output.Color = float4(finalColor, baseColor.a);
    output.Normal = float4(N, 1.0f); // alpha=1: 유효한 노말 마킹

    return output;
}
