# Exponential Height Fog 구현 문서

## 한 줄 요약

이 프로젝트의 Fog는 **깊이 버퍼를 읽어서 각 픽셀의 월드 위치를 복원한 뒤**, 카메라에서 그 픽셀까지 가는 선분 위의 안개 밀도를 **픽셀 셰이더에서 직접 적분**해서 계산하는 **포스트 프로세스 방식의 Exponential Height Fog**입니다.

즉, 메시를 안개로 따로 그리는 방식이 아니라, 이미 그려진 화면 위에 "이 픽셀은 안개를 얼마나 통과했는가?"를 계산해서 덧입히는 구조입니다.

## 관련 파일

- `KraftonEngine/Source/Engine/Component/ExponentialHeightFogComponent.h`
- `KraftonEngine/Source/Engine/Component/ExponentialHeightFogComponent.cpp`
- `KraftonEngine/Source/Engine/Render/Proxy/FScene.cpp`
- `KraftonEngine/Source/Engine/Render/Pipeline/RenderCollector.cpp`
- `KraftonEngine/Source/Engine/Render/Pipeline/RenderBus.h`
- `KraftonEngine/Source/Engine/Render/Pipeline/RenderConstants.h`
- `KraftonEngine/Source/Engine/Render/Pipeline/Renderer.cpp`
- `KraftonEngine/Source/Engine/Render/Device/BlendStateManager.cpp`
- `KraftonEngine/Shaders/Common/ConstantBuffers.hlsl`
- `KraftonEngine/Shaders/HeightFogPostProcess.hlsl`

## 전체 동작 흐름

### 1. Fog Actor/Component가 파라미터를 보관한다

`AExponentialHeightFog`는 루트로 `UExponentialHeightFogComponent`를 가집니다.

이 컴포넌트는 다음 값을 가지고 있습니다.

- `FogDensity`
- `FogHeightFalloff`
- `FogColor`
- `FogMaxOpacity`
- `StartDistance`
- `EndDistance`
- `FogCutoffDistance`

특이한 점은 `FogHeight`를 별도 변수로 저장하지 않고, **컴포넌트의 월드 위치 Z값**을 그대로 사용한다는 점입니다.

```cpp
float GetFogHeight() const { return GetWorldLocation().Z; }
```

즉, Fog 액터를 위아래로 움직이면 안개 기준 높이도 함께 바뀝니다.

### 2. 씬은 현재 사용할 Fog를 1개 고른다

Fog 컴포넌트는 렌더 상태 생성 시 씬에 등록되고, 파괴 시 해제됩니다.

현재 구현은 여러 Fog를 혼합하지 않고, `FScene::GetPrimaryExponentialHeightFog()`가 찾은 **첫 번째 활성 Fog 하나만 사용**합니다.

즉, 현재 구조는 "전역 Height Fog 1개"에 가깝습니다.

### 3. RenderCollector가 Fog 데이터를 RenderBus로 옮긴다

렌더 수집 단계에서 원근 카메라일 때만 Fog를 활성화합니다.

- 직교 카메라(`Ortho`)에서는 Fog를 적용하지 않습니다.
- 선택된 Fog 컴포넌트의 값을 `FExponentialHeightFogRenderData`로 복사합니다.

이 단계는 "게임 오브젝트 쪽 데이터"를 "렌더러가 쓰기 좋은 데이터"로 바꾸는 역할입니다.

### 4. Renderer가 Fog 포스트 프로세스 패스를 실행한다

`ERenderPass::FogPostProcess`는 렌더 패스 순서상 `Translucent` 다음에 실행됩니다.

이 패스는 다음 특징을 가집니다.

- 깊이 테스트를 하지 않습니다.
- 컬링을 하지 않습니다.
- **풀스크린 삼각형(full-screen triangle)** 1장을 그립니다.
- 깊이 버퍼를 픽셀 셰이더에서 읽습니다.
- Fog 전용 블렌드 상태를 사용합니다.

즉, Fog는 "화면 전체를 한 번 훑는 후처리 패스"입니다.

## 왜 픽셀 셰이더에서 구현하는가

Height Fog는 최종적으로 "카메라에서 물체까지 가는 동안 안개를 얼마나 통과했는가"를 각 픽셀마다 알아야 합니다.

이 값은 다음 이유로 픽셀 셰이더가 계산하기 좋습니다.

- 픽셀마다 깊이 값이 이미 있다.
- 깊이 값으로 그 픽셀의 3D 위치를 복원할 수 있다.
- 복원된 위치를 이용하면 카메라에서 그 지점까지의 실제 거리와 높이 변화를 계산할 수 있다.
- 계산 결과를 바로 화면 색과 합성할 수 있다.

이 구현은 별도의 볼륨 텍스처를 쓰거나 레이마칭을 여러 스텝 돌지 않습니다.  
대신, **지수 높이 분포를 수학적으로 바로 적분하는 닫힌 형태(closed-form)** 를 사용합니다.

이 점이 구현의 핵심입니다.

## 픽셀 셰이더가 하는 일

`HeightFogPostProcess.hlsl`의 `PS()`가 실제 Fog 계산을 담당합니다.

아래 순서로 동작합니다.

### 1. 현재 픽셀의 depth를 읽는다

```hlsl
float depth = DepthTexture.Sample(PointSampler, input.uv).r;
```

이 값은 "카메라에서 얼마나 먼가"가 아니라, 깊이 버퍼에 저장된 **device depth**입니다.

### 2. depth로 현재 픽셀의 월드 위치를 복원한다

셰이더는 `uv -> NDC -> clip space -> view space -> world space` 순서로 좌표를 복원합니다.

```hlsl
float2 ndcXY = float2(input.uv.x * 2.0f - 1.0f, 1.0f - input.uv.y * 2.0f);
float4 clipPos = float4(ndcXY, depth, 1.0f);
float4 viewPos = mul(clipPos, CameraInvProjection);
viewPos /= max(viewPos.w, 1e-6f);
float4 worldPos = mul(float4(viewPos.xyz, 1.0f), CameraInvView);
```

여기서 중요한 점은 Fog가 **깊이 버퍼를 기반으로 기존 장면 위에 덧칠되는 포스트 프로세스**라는 것입니다.

### 3. 카메라에서 그 픽셀까지의 ray를 만든다

```hlsl
float3 cameraToReceiver = worldPos.xyz - cameraPosition;
float fullDistance = length(cameraToReceiver);
```

여기서 `receiver`는 "현재 픽셀에 이미 그려져 있는 표면 위치"라고 보면 됩니다.

즉, Fog는 카메라에서 그 표면까지 가는 구간에 대해서만 계산합니다.

### 4. 시작 거리, 끝 거리, 컷오프 거리를 적용한다

현재 구현은 Fog 적용 구간을 세 가지 방식으로 제한합니다.

#### `StartDistance`

카메라 바로 앞 구간에서는 Fog를 빼고 싶을 때 씁니다.

셰이더는 ray 시작점을 카메라 위치에서 조금 앞으로 민 뒤, 그 이후 구간만 적분합니다.

```hlsl
rayOrigin += rayDirection * startDistance;
rayLength -= startDistance;
```

#### `EndDistance`

Fog가 너무 멀리까지 가지 않게 하고 싶을 때 씁니다.

주의할 점은 현재 구현이 **3D 거리**가 아니라 **수평 거리(XY 길이)** 기준으로 잘라낸다는 점입니다.

```hlsl
float rayXYLength = length(cameraToReceiver.xy);
if (FogEndDistance > 0.0f && rayXYLength > FogEndDistance)
{
    rayScale = FogEndDistance / max(rayXYLength, 1e-6f);
}
```

즉, 위아래 높이 차이보다 "화면 속에서 얼마나 멀리 가는가"를 기준으로 제한합니다.

#### `FogCutoffDistance`

이 값보다 멀리 있는 픽셀은 Fog를 아예 계산하지 않습니다.

```hlsl
if (FogCutoffDistance > 0.0f && fullDistance > FogCutoffDistance)
{
    return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
```

이 반환값은 블렌드 결과상 "원래 화면을 그대로 유지"한다는 뜻입니다.

## Fog 밀도 모델

이 Fog는 높이에 따라 밀도가 줄어드는 전형적인 Exponential Height Fog 모델입니다.

밀도는 개념적으로 아래처럼 볼 수 있습니다.

```text
Density(z) = FogDensity * 2^(-FogHeightFalloff * (z - FogHeight))
```

의미는 다음과 같습니다.

- `FogDensity`: 전체 안개의 기본 진하기
- `FogHeight`: 안개의 기준 높이
- `FogHeightFalloff`: 높이가 올라갈수록 얼마나 빨리 엷어질지

즉:

- `z`가 `FogHeight`보다 높아질수록 안개가 빠르게 옅어집니다.
- `FogHeightFalloff`가 클수록 위로 갈 때 더 빨리 사라집니다.
- `FogHeight` 아래쪽은 상대적으로 더 짙어집니다.

## 이 구현이 중요한 이유: 레이마칭이 아니라 "직접 적분"이다

많은 볼류메트릭 효과는 ray를 여러 스텝으로 잘라서 누적합니다.  
하지만 이 구현은 그 방식이 아닙니다.

이 프로젝트의 Fog는 ray 위의 밀도를 **수식으로 바로 적분**합니다.

셰이더 코드:

```hlsl
float rayOriginTerms = density * exp2(-heightFalloff * (rayOrigin.z - fogBaseHeight));
float integral = rayOriginTerms * CalculateLineIntegralShared(heightFalloff, rayDirection.z, rayLength);
float transmittance = exp2(-integral);
```

보조 함수:

```hlsl
float CalculateLineIntegralShared(float heightFalloff, float rayDirectionZ, float rayLength)
{
    float falloff = heightFalloff * rayDirectionZ;
    if (abs(falloff) < 1e-4f)
    {
        return rayLength;
    }

    float exponent = exp2(-falloff * rayLength);
    return (1.0f - exponent) / (falloff * 0.69314718056f);
}
```

의미를 풀어쓰면 이렇습니다.

1. ray 시작점에서의 안개 밀도를 구한다.
2. ray가 위로 가는지 아래로 가는지에 따라 밀도 변화량을 정한다.
3. 그 변화를 선분 전체에 대해 적분한다.
4. 그 적분값으로 "빛이 얼마나 살아남는가"를 계산한다.

이 방식의 장점:

- 픽셀당 계산량이 비교적 작습니다.
- step 수를 늘리거나 줄일 필요가 없습니다.
- 밴딩이나 샘플 수 부족 문제를 크게 줄일 수 있습니다.

## 최종 색 계산

셰이더는 최종적으로 두 값을 만듭니다.

```hlsl
float transmittance = exp2(-integral);
transmittance = max(saturate(transmittance), 1.0f - FogMaxOpacity);

float3 fogContribution = FogColor.rgb * (1.0f - transmittance);
return float4(fogContribution, transmittance);
```

여기서 의미는 다음과 같습니다.

- `transmittance`: 원래 장면 색이 얼마나 남는가
- `fogContribution`: 안개 색이 얼마나 더해질 것인가

`FogMaxOpacity`는 "안개가 아무리 진해도 이 이상은 화면을 덮지 마라"라는 상한입니다.

예를 들어:

- `FogMaxOpacity = 1.0`이면 완전히 덮을 수 있습니다.
- `FogMaxOpacity = 0.6`이면 원래 장면 색이 최소 40%는 남습니다.

## 블렌드 단계에서 실제 화면에 합성되는 방식

Fog 패스는 별도의 블렌드 상태를 씁니다.

```cpp
SrcBlend  = D3D11_BLEND_ONE;
DestBlend = D3D11_BLEND_SRC_ALPHA;
```

셰이더 출력이 `float4(fogContribution, transmittance)`이므로, 최종 식은 다음과 같습니다.

```text
FinalColor = fogContribution + SceneColor * transmittance
```

즉, 이 프로젝트의 Fog는 구조적으로 아래 두 단계로 나뉩니다.

1. 픽셀 셰이더가 "안개색"과 "원본 보존율"을 계산한다.
2. 블렌드 상태가 둘을 실제 프레임 버퍼에 합성한다.

이 분리가 꽤 중요합니다.  
픽셀 셰이더 안에서 최종 화면색을 직접 읽고 다시 쓰는 것이 아니라, **Fog 기여분과 투과율만 계산하고 블렌더에게 합성을 맡기는 구조**이기 때문입니다.

## 상수 버퍼에 들어가는 값

Fog 패스는 `b6` 슬롯의 상수 버퍼를 사용합니다.

```hlsl
cbuffer HeightFogPostProcessCB : register(b6)
{
    float4x4 CameraInvView;
    float4x4 CameraInvProjection;
    float4 FogColor;
    float3 FogCameraPosition;
    float FogDensity;
    float FogHeight;
    float FogHeightFalloff;
    float FogStartDistance;
    float FogMaxOpacity;
    float FogEndDistance;
    float FogCutoffDistance;
    float2 FogPadding;
};
```

렌더러는 매 프레임 이 값을 채워서 셰이더에 넘깁니다.

특히 중요한 값은 아래 네 가지입니다.

- `CameraInvProjection`: depth에서 view-space를 복원하기 위해 필요
- `CameraInvView`: view-space를 world-space로 바꾸기 위해 필요
- `FogCameraPosition`: 카메라에서 픽셀까지의 ray를 만들기 위해 필요
- Fog 파라미터들: 실제 밀도 계산에 필요

## 풀스크린 삼각형 방식

Fog 패스는 버텍스 버퍼 없이 `Draw(3, 0)`만 호출합니다.

버텍스 셰이더는 `SV_VertexID`만으로 화면 전체를 덮는 삼각형을 만듭니다.

```hlsl
output.uv = float2((vertexID << 1) & 2, vertexID & 2);
output.position = float4(output.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
```

이 방식의 장점:

- 정점 버퍼가 필요 없습니다.
- 사각형 2장 대신 삼각형 1장만 그립니다.
- 포스트 프로세스 구현이 단순해집니다.

## 파라미터별 체감 효과

### `FogDensity`

- 값을 올리면 전체적으로 더 빨리 뿌옇게 됩니다.
- 거리와 상관없이 "기본 농도" 자체가 커집니다.

### `FogHeightFalloff`

- 값을 올리면 높은 곳에서 안개가 더 빨리 사라집니다.
- 값이 0에 가까우면 높이에 거의 반응하지 않는 균일한 안개처럼 보입니다.

### `FogHeight`

- Fog 액터의 월드 Z값입니다.
- 액터를 위로 올리면 안개 기준면도 올라갑니다.

### `StartDistance`

- 카메라 가까운 곳을 맑게 유지합니다.
- 1인칭/에디터 카메라에서 답답함을 줄이는 데 유용합니다.

### `EndDistance`

- 수평 방향으로 Fog가 적용되는 최대 범위를 제한합니다.
- 현재 구현은 XY 거리 기준이라, 높은 곳을 보는 상황에서도 수평 거리 기준으로 잘립니다.

### `FogCutoffDistance`

- 이 거리 밖은 Fog를 아예 적용하지 않습니다.
- 멀리 있는 배경을 깔끔하게 남기고 싶을 때 유용합니다.

### `FogMaxOpacity`

- 안개가 덮을 수 있는 최대 비율입니다.
- 값을 낮추면 항상 원래 화면 정보가 일부 남습니다.

### `FogColor`

- 안개의 색입니다.
- 셰이더는 산란의 방향성이나 광원 색을 따로 계산하지 않고, 이 색을 그대로 사용합니다.

## 현재 구현의 특징과 한계

### 장점

- 구현이 단순하고 비용이 비교적 낮습니다.
- 깊이 버퍼만 있으면 동작하므로 기존 렌더링 파이프라인에 붙이기 쉽습니다.
- 높이에 따른 안개 변화가 자연스럽습니다.
- 직접 적분 방식이라 스텝 기반 레이마칭보다 깔끔합니다.

### 현재 한계

- 활성 Fog를 여러 개 섞지 않고 **하나만 사용**합니다.
- 광원에 따른 산란, 볼류메트릭 섀프트, 노이즈, 다중 산란은 없습니다.
- 3D 공간 전체의 안개 부피를 샘플링하는 진짜 volumetric fog는 아닙니다.
- `EndDistance`가 3D 거리 기준이 아니라 XY 기준입니다.
- 원근 카메라에서만 동작하고, 직교 카메라는 비활성화됩니다.
- 안개 색은 단색이며 높이에 따라 색이 바뀌지는 않습니다.

## 이해를 돕는 요약

이 Fog는 본질적으로 아래 공식으로 이해하면 됩니다.

```text
1. depth로 현재 픽셀의 월드 위치를 복원한다.
2. 카메라에서 그 위치까지 ray를 만든다.
3. ray 위의 안개 밀도(높이에 따라 지수적으로 변함)를 적분한다.
4. 적분 결과를 transmittance로 바꾼다.
5. FogColor와 원래 SceneColor를 블렌드해서 최종 화면을 만든다.
```

수식으로 가장 짧게 적으면:

```text
Density(z)      = FogDensity * 2^(-FogHeightFalloff * (z - FogHeight))
Transmittance   = 2^(-IntegralAlongRay)
FogContribution = FogColor * (1 - Transmittance)
FinalColor      = FogContribution + SceneColor * Transmittance
```

즉, "멀수록 더 많이 흐려지고, 높을수록 안개가 옅어지는 색 보정"을 깊이 기반 포스트 프로세스로 구현한 것입니다.
