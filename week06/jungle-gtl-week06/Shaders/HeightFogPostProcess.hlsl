#include "Common/ConstantBuffers.hlsl"

Texture2D<float> DepthTexture : register(t0);
SamplerState PointSampler : register(s0);

static const float LOG2_EPSILON = 0.69314718056f;

struct PS_Input
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

PS_Input VS(uint vertexID : SV_VertexID)
{
    PS_Input output;
    output.uv = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return output;
}

float CalculateLineIntegralShared(float heightFalloff, float rayDirectionZ, float rayLength)
{
    float falloff = heightFalloff * rayDirectionZ;
    if (abs(falloff) < 1e-4f)
    {
        return rayLength;
    }

    float exponent = exp2(-falloff * rayLength);
    return (1.0f - exponent) / (falloff * LOG2_EPSILON);
}

float4 PS(PS_Input input) : SV_TARGET
{
    // 현재 화면 좌표(uv)에 해당하는 depth 값을 읽는다.
    // 이 값은 "실제 거리"가 아니라 depth buffer에 저장된 device depth다.
    float depth = DepthTexture.Sample(PointSampler, input.uv).r;

    // depth가 0이거나 거의 1이면, 현재 픽셀에는 유효한 geometry가 없다고 본다.
    // 그래서 지금 구현은 배경/clear color에는 fog를 적용하지 않는다.
    if (depth <= 0.0f)
    {
        // rgb=0은 "추가 안개색 없음", a=1은 "기존 화면색을 그대로 유지"를 뜻한다.
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // uv(0~1)를 NDC(-1~1) 좌표로 바꾼다.
    // y는 텍스처 좌표와 화면 좌표 방향이 반대라 뒤집어 준다.
    float2 ndcXY = float2(input.uv.x * 2.0f - 1.0f, 1.0f - input.uv.y * 2.0f);

    // 복원에 사용할 clip-space 좌표를 만든다.
    // x,y는 방금 만든 NDC, z는 depth buffer에서 읽은 값, w는 1이다.
    float4 clipPos = float4(ndcXY, depth, 1.0f);

    // inverse projection으로 clip-space -> view-space 위치를 복원한다.
    float4 viewPos = mul(clipPos, CameraInvProjection);

    // homogeneous divide로 진짜 view-space 위치를 얻는다.
    viewPos /= max(viewPos.w, 1e-6f);

    // inverse view로 view-space -> world-space 위치를 복원한다.
    float4 worldPos = mul(float4(viewPos.xyz, 1.0f), CameraInvView);

    // 카메라 월드 위치.
    float3 cameraPosition = FogCameraPosition;

    // 카메라에서 현재 픽셀의 실제 월드 위치까지 가는 벡터.
    float3 cameraToReceiver = worldPos.xyz - cameraPosition;

    // 카메라에서 현재 픽셀까지의 전체 거리.
    float fullDistance = length(cameraToReceiver);

    // 너무 가까우면 사실상 같은 점이므로 fog 계산을 생략한다.
    if (fullDistance <= 1e-4f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // cutoff distance가 설정돼 있고, 현재 픽셀이 그보다 멀면 fog를 아예 적용하지 않는다.
    if (FogCutoffDistance > 0.0f && fullDistance > FogCutoffDistance)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // 기본적으로는 카메라 -> 픽셀 전체 구간을 안개 적분에 사용한다.
    float rayScale = 1.0f;

    // EndDistance는 수평(XY) 거리 기준으로 fog가 쌓이는 최대 범위를 제한하는 용도다.
    float rayXYLength = length(cameraToReceiver.xy);
    if (FogEndDistance > 0.0f && rayXYLength > FogEndDistance)
    {
        // 너무 멀면 ray를 줄여서 EndDistance까지만 fog를 계산한다.
        rayScale = FogEndDistance / max(rayXYLength, 1e-6f);
    }

    // 실제 fog 계산에 사용할 ray.
    float3 fogRay = cameraToReceiver * rayScale;

    // fog ray 길이.
    float rayLength = length(fogRay);
    if (rayLength <= 1e-4f)
    {
        return float4(0.0f, 0.0f, 0.0f, 1.0f);
    }

    // ray 방향 벡터를 정규화한다.
    float3 rayDirection = fogRay / rayLength;

    // StartDistance는 카메라 바로 앞 일정 거리까지는 fog를 제외하기 위한 값이다.
    float startDistance = min(FogStartDistance, rayLength);

    // 적분 시작점은 기본적으로 카메라 위치다.
    float3 rayOrigin = cameraPosition;

    if (startDistance > 0.0f)
    {
        // 시작 거리가 있으면, 카메라에서 조금 전진한 지점부터 fog 계산을 시작한다.
        rayOrigin += rayDirection * startDistance;

        // 그만큼 남은 적분 길이를 줄인다.
        rayLength -= startDistance;
        if (rayLength <= 1e-4f)
        {
            return float4(0.0f, 0.0f, 0.0f, 1.0f);
        }
    }

    // density: 안개의 기본 세기
    float density = FogDensity;

    // heightFalloff: 높이 올라갈수록 안개가 얼마나 빨리 줄어드는지
    float heightFalloff = FogHeightFalloff;

    // fogBaseHeight: 안개 기준 높이 (현재는 fog actor의 world Z)
    float fogBaseHeight = FogHeight;

    // 현재 적분 시작점(rayOrigin)에서의 안개 밀도를 계산한다.
    // 높이가 fogBaseHeight보다 높을수록 exp2 항 때문에 안개가 약해진다.
    float rayOriginTerms = density * exp2(-heightFalloff * (rayOrigin.z - fogBaseHeight));

    // ray 전체 구간에서 안개가 얼마나 누적되는지 적분한다.
    float integral = rayOriginTerms * CalculateLineIntegralShared(heightFalloff, rayDirection.z, rayLength);

    // transmittance는 "원래 화면색이 얼마나 살아남는가"이다.
    // 적분값이 커질수록 더 탁해지고, 원래 색은 덜 보인다.
    float transmittance = exp2(-integral);

    // FogMaxOpacity를 넘어서 너무 짙어지지 않도록 최소 투과율을 보장한다.
    transmittance = max(saturate(transmittance), 1.0f - FogMaxOpacity);

    // 최종 fogContribution은 "원래 색 위에 덧씌울 안개색 양"이다.
    float3 fogContribution = FogColor.rgb * (1.0f - transmittance);

    // 반환 규칙:
    // rgb = 추가할 안개색
    // a   = 기존 화면색을 얼마나 남길지(투과율)
    // blend state에서 최종적으로 FogColor + SceneColor * Transmittance 형태로 합성된다.
    return float4(fogContribution, transmittance);
}
