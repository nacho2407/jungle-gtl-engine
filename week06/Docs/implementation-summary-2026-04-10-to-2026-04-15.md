# 구현 요약 문서 (2026-04-10 ~ 2026-04-15)

## 범위

- 시작 커밋: `4f014ea` (`Add: UDecalComponent`, 2026-04-10)
- 종료 커밋: `da19ace` (`feat: 충돌 normal 반영`, 2026-04-15)
- 이 문서는 위 범위에 포함된 구현 내용을 기능 단위로 정리한 문서입니다.
- 단순 merge 커밋이나 충돌 해결 커밋은 구조나 동작에 영향이 있는 경우만 맥락에 포함했습니다.

## 전체 요약

이 구간에서는 데칼을 시작으로 화면 공간 기반 효과, 충돌 기반 게임플레이 오브젝트, 그리고 이를 다루기 위한 에디터 기능이 한꺼번에 확장되었습니다.

- `UDecalComponent`를 중심으로 한 Projection Decal 파이프라인이 추가되었습니다.
- 깊이/노말 버퍼를 활용하는 Fireball 효과가 추가되었습니다.
- Depth 기반 World Position 복원을 사용하는 Exponential Height Fog가 추가되었습니다.
- Projectile/Egg 흐름에 충돌 처리와 충돌 지점 데칼 생성이 연결되었습니다.
- 에디터에는 Spawn 메뉴, Show Flag, Stat, SceneDepth 뷰, Selection Mask, FXAA 설정 등 디버깅과 시각화 기능이 보강되었습니다.

## 1. Decal 시스템

### 무엇이 추가되었는가

데칼 시스템은 새로운 컴포넌트와 액터에서 시작해서, 최종적으로는 별도 렌더 패스를 가진 구조로 확장되었습니다.

- `UDecalComponent`가 추가되었습니다.
  - 데칼 크기, 색상, 텍스처, Fade 설정, Hit Test 등을 보유합니다.
- `ADecalActor`가 추가되었습니다.
  - 씬에서 데칼을 직접 배치할 수 있는 액터 래퍼입니다.
- `FDecalSceneProxy`가 추가되었습니다.
  - 데칼 데이터를 렌더러로 넘기는 프록시 역할을 합니다.
- 데칼 전용 셰이더, 상수 버퍼, 렌더 패스, 블렌드 상태가 추가되었습니다.

대표 커밋:

- `4f014ea` `Add: UDecalComponent`
- `85ab3f8` `Init DecalActor`
- `2db7568` `Init DecalSceneProxy`
- `8387a69` `add: Decal Shader/ConstantBuffer`
- `73fa83e` `Add Decal render pass and state`

### 현재 구조에서 어떻게 동작하는가

현재 데칼 경로는 "데칼 볼륨만 그린다"기보다는, "렌더 가능한 receiver를 먼저 고른 뒤 해당 receiver에 데칼을 그린다"는 구조에 가깝습니다.

1. `FRenderCollector::CollectDecals()`가 보이는 Static Mesh receiver와 보이는 Decal Component를 순회합니다.
2. 월드 Octree를 이용해 각 데칼 주변의 후보 receiver를 Broad Phase로 수집합니다.
3. `FIntersectionUtils::IntersectOBBAndAABB()`를 이용해 데칼 박스와 receiver 박스의 교차 여부를 Narrow Phase로 판정합니다.
4. 통과한 receiver/decal 쌍에 대해 `FDecalDrawEntry`를 `FRenderBus`에 넣습니다.
5. `FRenderer::DrawDecalPass()`가 receiver mesh를 데칼 셰이더로 다시 그리면서 `WorldToDecal` 변환을 적용합니다.

즉, 모든 데칼을 모든 오브젝트에 무작정 그리는 방식이 아니라, "실제로 겹칠 가능성이 있는 receiver만 골라서" 그리도록 설계되었습니다.

관련 파일:

- `KraftonEngine/Source/Engine/Component/DecalComponent.h`
- `KraftonEngine/Source/Engine/Render/Proxy/DecalSceneProxy.cpp`
- `KraftonEngine/Source/Engine/Render/Pipeline/RenderCollector.cpp`
- `KraftonEngine/Source/Engine/Collision/IntersectionUtils.cpp`
- `KraftonEngine/Shaders/Decal.hlsl`
- `KraftonEngine/Source/Engine/Render/Pipeline/Renderer.cpp`

### 이 범위에서 보강된 내용

- `DecalToWorld`, `WorldToDecal` 캐시 행렬이 추가되었고 transform dirty 처리 문제가 수정되었습니다.
- Projection Decal 셰이더 로직이 구현되었습니다.
- Fade In / Fade Out / Lifetime 기반 소멸 흐름이 추가되었습니다.
- 알파 조절 시 데칼이 과하게 타는 문제를 블렌드 상태 수정으로 보완했습니다.
- 데칼 수신 옵션이 `UPrimitiveComponent`에서 `UStaticMeshComponent`로 이동했습니다.
- 전역 Decal Show Flag와 개별 receiver 옵션이 추가되었습니다.
- 데칼 통계가 추가되어 visible decals, broad candidates, SAT accepted, submitted draws, rendered draws를 확인할 수 있게 되었습니다.
- 데칼 볼륨 와이어프레임과 투영 방향 화살표 디버그 표시가 추가되었습니다.

대표 커밋:

- `3b5b608` `Add cached DecalToWorld/WorldToDecal matrices`
- `999ce6b` `fix: Decal 행렬 갱신 안되는 문제 해결`
- `79621bc` `Projection Decal 구현`
- `0bf1bee` `add: Decal Fade In/Out`
- `71a6f5e` `fix: 데칼 알파값 조정 시 데칼이 타버리는 문제 해결`
- `b3f30cd` `add: 전역 Decals show flag, primitive별 Decal Recieve flag 추가`
- `18a5ade` `fix: decal 수신 옵션을 primitive 컴포넌트에서 static mesh 컴포넌트로 이동`
- `9313f81` `add: decal과의 교차 여부 검사 추가`
- `2fd3258` `add: decal stat`
- `30f3213` `Feat: Decal Stat 추가`
- `0ecf946` `add: decal showflag`
- `f6fc5e5` `hotfix: Decal Pass resource binding error`

## 2. Fireball 효과

### 무엇이 추가되었는가

`UFireballComponent`와 `FFireballSceneProxy`를 중심으로, 일반적인 메쉬 재질과는 다른 화면 공간 기반 효과가 추가되었습니다.

- 컴포넌트는 `Intensity`, `Radius`, `RadiusFalloff`, `Color`를 노출합니다.
- 프록시는 뷰포트마다 Fireball 상수 버퍼를 갱신합니다.
- 효과는 `Additive` 패스에서 렌더링됩니다.

대표 커밋:

- `625facb` `Init FireballComponent`
- `4dc72da` `Add Fireball scene proxy and render constants`
- `5fc9bae` `add Fireball property`
- `3e3dc24` `add: binding fireball shader`

### 현재 구조에서 어떻게 동작하는가

Fireball은 일반 Opaque Mesh처럼 그려지는 것이 아니라, Fullscreen Triangle 기반 Additive 효과로 처리됩니다.

1. Fireball 프록시가 `ERenderPass::Additive`로 들어갑니다.
2. Additive 패스가 Scene Albedo, Depth, Normal SRV를 셰이더에 바인딩합니다.
3. Fireball 셰이더가 현재 픽셀의 depth를 이용해 world position을 복원합니다.
4. 그 월드 좌표와 fireball 중심 사이의 거리를 계산합니다.
5. 반지름 밖 픽셀은 버립니다.
6. 반지름 안의 픽셀은 거리 감쇠와 노말 기반 조명량을 계산해서 장면 위에 additive로 합성합니다.

즉, Fireball은 "구체 메쉬 자체를 발광시키는 방식"이라기보다, 이미 그려진 장면 버퍼를 바탕으로 후처리처럼 광량을 더하는 구조입니다.

관련 파일:

- `KraftonEngine/Source/Engine/Component/FireballComponent.h`
- `KraftonEngine/Source/Engine/Render/Proxy/FireballSceneProxy.cpp`
- `KraftonEngine/Shaders/Fireball.hlsl`
- `KraftonEngine/Source/Engine/Render/Pipeline/Renderer.cpp`
- `KraftonEngine/Source/Engine/Render/Pipeline/RenderBus.h`

### 이 범위에서 보강된 내용

- Fireball 셰이더가 장면 노말과 알베도 정보를 사용할 수 있도록 MRT 출력이 확장되었습니다.
- 거리 기반 falloff와 경계 감쇠가 개선되었습니다.
- 최종 블렌드 방식이 additive로 정리되었습니다.
- Outline 비지원으로 분리되었습니다.
- 컬링 문제를 수정했고 테스트용 Scene이 추가되었습니다.
- Color 저장 타입이 `FVector4` 성격의 사용에서 `FVector`로 정리되었습니다.

대표 커밋:

- `04792fd` `Add additive fireball render pass`
- `86ab124` `Add normal RTV/SRV and MRT output`
- `a815645` `Use normal map for lighting in fireball shader`
- `c93c605` `Add Albedo G-buffer MRT and bindings`
- `184e698` `Improve fireball falloff and lighting`
- `66f48ca` `fireball component가 outline을 미지원하도록 수정`
- `6855ce0` `fix: FireballComponent가 컬링되는 문제 해결 + Fireball 씬 추가`
- `323d226` `change fireball blendstate to additive`
- `c2b6695` `Use FVector for Fireball color`

## 3. Exponential Height Fog

### 무엇이 추가되었는가

Exponential Height Fog가 화면 공간 기반 Post Process 효과로 추가되었습니다.

- `AExponentialHeightFog`와 `UExponentialHeightFogComponent`가 추가되었습니다.
- `FScene`가 현재 사용할 Fog Component를 관리합니다.
- `FRenderCollector`가 fog 파라미터를 `FRenderBus`에 수집합니다.
- `FRenderer::DrawPostProcessFog()`가 depth buffer를 기반으로 fog 셰이더를 실행합니다.

대표 커밋:

- `8bb9917` `feat: 지수 높이 포그 기본 경로 추가`

### 현재 구조에서 어떻게 동작하는가

Fog 패스는 각 픽셀의 depth로부터 world position을 복원한 뒤, 카메라에서 그 픽셀까지의 ray를 따라 fog를 계산합니다.

- 셰이더는 inverse view / inverse projection 행렬과 fog 파라미터를 받습니다.
- `uv + depth`로부터 `worldPos`를 복원합니다.
- 카메라에서 해당 위치까지의 ray를 계산합니다.
- 높이에 따라 지수적으로 변하는 density와 거리 누적량을 계산합니다.
- 최종적으로 fog contribution과 transmittance를 만들어 scene color와 합성합니다.

Fog 수식 자체는 이미 별도 문서로 정리되어 있으므로 여기서는 구조만 요약합니다.

참고 문서:

- `Docs/exponential-height-fog.md`

관련 파일:

- `KraftonEngine/Source/Engine/Component/ExponentialHeightFogComponent.h`
- `KraftonEngine/Source/Engine/Render/Pipeline/RenderCollector.cpp`
- `KraftonEngine/Shaders/HeightFogPostProcess.hlsl`
- `KraftonEngine/Source/Engine/Render/Pipeline/Renderer.cpp`

관련 커밋:

- `fa29bc6` `Create exponential-height-fog.md`

## 4. Projectile, Egg, 충돌 기반 데칼

### 무엇이 추가되었는가

Projectile 흐름이 단순 이동에서 충돌 처리 가능한 구조로 확장되었고, 그 위에 `AEggActor`가 테스트/게임플레이 오브젝트로 추가되었습니다.

- `AEggActor`는 `AStaticMeshActor`를 상속합니다.
- `UProjectileMovementComponent`를 붙입니다.
- 충돌 시 메시를 숨기고 데칼을 남기는 흐름이 추가되었습니다.

대표 커밋:

- `a751c1c` `Feat: EggActor추가`

### 현재 구조에서 어떻게 동작하는가

`UProjectileMovementComponent`는 매 tick마다 swept movement 기반으로 충돌을 검사합니다.

1. 중력과 최대 속도를 반영해 velocity를 갱신합니다.
2. 이동 전후를 포함하는 swept AABB를 만듭니다.
3. Spatial Partition에서 후보 primitive를 가져옵니다.
4. 각 후보에 대해 line trace를 수행합니다.
5. 가장 가까운 blocking hit를 선택합니다.
6. 충돌이 나면 이동을 멈추고 hit behavior를 수행합니다.

`AEggActor`의 경우 hit behavior는 다음과 같습니다.

- Egg mesh를 숨깁니다.
- 기존 `UDecalComponent`를 찾거나 새로 생성합니다.
- `Friedegg` 텍스처를 설정합니다.
- 충돌 위치에 데칼을 배치합니다.
- 충돌 normal 방향으로 데칼을 정렬합니다.
- Fade In / Fade Out / 소멸 설정을 적용합니다.

관련 파일:

- `KraftonEngine/Source/Engine/Component/ProjectileMovementComponent.cpp`
- `KraftonEngine/Source/Engine/GameFramework/EggActor.cpp`
- `KraftonEngine/Source/Engine/Core/CollisionTypes.h`
- `KraftonEngine/Source/Engine/Collision/MeshTrianglePickingBVH.cpp`

### 이 범위에서 보강된 내용

- 초기 충돌 구현이 한 번 들어갔다가 revert 되었습니다.
- 이후 현재 구조에 가까운 swept-query 방식으로 다시 정리되었습니다.
- 충돌 노말이 `FHitResult`에 반영되어, 충돌면 방향에 맞춘 데칼 배치가 가능해졌습니다.

대표 커밋:

- `f2be1ca` `Feat: ProjectileComponent collision추가`
- `1d4d239` `Revert "Feat: ProjectileComponent collision추가"`
- `da19ace` `feat: 충돌 normal 반영`

## 5. 렌더링 인프라 확장

이 구간에서는 개별 기능 외에도, 새로운 효과를 받쳐주기 위한 렌더러 구조 변화가 같이 들어갔습니다.

### 추가되거나 확장된 렌더 패스

- `Decal`
- `Additive`
- `FogPostProcess`
- `SelectionMask`
- `SceneDepthProcess`
- `AntiAliasing`

### 주요 인프라 변화

- 패스별 Resource Binding Hook이 추가되어, 패스 단위로 SRV/RTV 바인딩과 해제가 가능해졌습니다.
- `ExecutePass` 시그니처가 단순화되었습니다.
- Fullscreen Triangle Draw Mode가 추가되어 Post Process 계열 효과나 화면 공간 효과를 프록시 기반으로 처리할 수 있게 되었습니다.
- Selection Mask 렌더링이 추가되어 Outline Post Process가 선택된 오브젝트만 분리해서 다룰 수 있게 되었습니다.
- Scene Depth 시각화 View Mode가 추가되었습니다.

대표 커밋:

- `dc88434` `Add per-pass resource binding hooks`
- `a52ba92` `Simplify ExecutePass signature`
- `2cef084` `Add fullscreen-triangle draw mode for proxies`
- `d5701bb` `add: selection mask shader`
- `201db29` `feat: SceneDepthBuffer ViewMode 기능 추가`

## 6. FXAA와 뷰포트 후처리 설정

FXAA가 별도의 후처리 단계와 에디터 설정으로 추가되었습니다.

### 무엇이 추가되었는가

- FXAA 셰이더와 상수 버퍼
- Editor Settings 저장/로드
- Viewport 단위 enable/disable 옵션
- 별도 FXAA 설정 위젯

대표 커밋:

- `4c78510` `add: FXAA 셰이더 추가`
- `831adc3` `chore: move FXAA shader file`
- `e308422` `add: fxaa first version`
- `5bf0709` `fix: fxaa shader`
- `8c3e449` `modify: FXAA 설정 UI 분리`

관련 파일:

- `KraftonEngine/Shaders/FXAA.hlsl`
- `KraftonEngine/Source/Editor/UI/EditorFXAAWidget.cpp`
- `KraftonEngine/Source/Editor/Settings/EditorSettings.cpp`
- `KraftonEngine/Source/Engine/Render/Pipeline/Renderer.cpp`

## 7. 에디터 / 디버그 툴링

새로 추가된 효과와 오브젝트를 다루기 위한 에디터 기능도 이 구간에서 많이 보강되었습니다.

### 추가되거나 변경된 에디터 기능

- Decal, Fog, Egg, Spotlight 계열 오브젝트 Spawn 메뉴
- Spawn된 Actor 자동 선택
- Scene Widget의 물체 찾기 기능
- Stat Widget / Overlay의 Decal 통계 표시
- Viewport Toolbar의 SceneDepthBuffer 뷰 모드
- Decal 관련 Show Flag
- Decal Projection Arrow / Volume Wireframe 디버그 표시

대표 커밋:

- `2eab9ad` `Add: (디버깅용) DecalActor option in control widget`
- `6fdce49` `modify: 스폰된 actor들은 모두 선택 상태가 되도록 수정`
- `6e014f3` `feat: 물체 찾기`
- `2fd3258` `add: decal stat`
- `30f3213` `Feat: Decal Stat 추가`
- `bdee773` `feat: decal projection 방향 지시선 추가`

### 함께 들어간 품질 개선

- Billboard가 부모 스케일 영향을 무시하는 옵션이 추가되었습니다.
- `TextRenderComponent`가 생성자에서 기본 폰트를 초기화하도록 정리되었습니다.
- Grid가 먼저 그려지던 순서 문제가 수정되었습니다.
- Gizmo 회전 조작 중 자동 회전이 발생하던 문제가 수정되었습니다.

대표 커밋:

- `8d3ee24` `feat: 부모 actor가 스케일링 되더라도 billboard component는 변하지 않도록 하는 옵션 추가`
- `26a4cf3` `Initialize font in TextRenderComponent constructor`
- `baebfbb` `fix: grid먼저 그려지는 버그 수정`
- `74b5e3b` `fix: 기즈모 회전 조작 도중 자동으로 회전하던 문제 해결`

## 8. Spotlight Actor

`ASpotlightActor`는 완전한 실시간 조명 시스템이라기보다, 기존 요소를 조합해 만든 "Spotlight처럼 보이는" 복합 액터입니다.

- Root로 Billboard Component를 사용합니다.
- 여기에 `UDecalComponent`를 붙여 스포트라이트 모양의 텍스처를 투영합니다.
- `UTextRenderComponent`로 UUID 텍스트를 같이 표시합니다.

즉, 이 액터는 실제 동적 조명을 추가한 것이 아니라, 데칼과 빌보드를 조합해 에디터/런타임에서 light-like actor를 만들 수 있게 한 구현입니다.

대표 커밋:

- `aa6fe0a` `feat: spotlight actor 구현`

관련 파일:

- `KraftonEngine/Source/Engine/GameFramework/SpotlightActor.cpp`
- `KraftonEngine/Asset/Textures/spotlight_Light.png`
- `KraftonEngine/Asset/Textures/spotlight_Spot.png`

## 9. Rotating Movement와 Tick / Runtime 통합

이 범위에서는 단순 렌더링 효과 외에도 runtime 동작을 안정화하기 위한 구조 변경이 같이 들어갔습니다.

### Rotating Movement

`URotatingMovementComponent`가 추가되어 pivot을 기준으로 지속 회전하는 동작을 quaternion 기반으로 처리할 수 있게 되었습니다.

대표 커밋:

- `fac54d2` `feat: rotating 기능 추가`

### Tick / Runtime 흐름 정리

Tick Manager 관련 수정이 들어가면서, 새로 추가된 component와 runtime behavior가 월드 업데이트 흐름에 더 안정적으로 참여하게 되었습니다.

대표 커밋:

- `1212991` `Fix: TickManager수정`

이 변경은 단독으로 보이면 작아 보이지만, 실제로는 데칼 fade, projectile, rotating movement처럼 tick에 의존하는 기능들을 안정적으로 돌리는 기반 역할을 합니다.

## 10. Scene / Asset / Serialization 적용 범위

이 구간에서는 기능만 추가된 것이 아니라, 이를 바로 확인할 수 있는 Scene / Texture / Asset도 함께 추가되었습니다.

추가된 대표 Scene:

- `KraftonEngine/Asset/Scene/FireballScene.Scene`
- `KraftonEngine/Asset/Scene/Week6_Decal.Scene`
- `KraftonEngine/Asset/Scene/Week6_EggScene.Scene`
- `KraftonEngine/Asset/Scene/SpotLightFake.Scene`

또한 기존 `FSceneSaveManager`의 editable property 기반 직렬화 구조를 통해, 새로 추가된 component-backed actor들도 Scene 저장/로드 흐름에 자연스럽게 편입되었습니다.

관련 파일:

- `KraftonEngine/Source/Engine/Serialization/SceneSaveManager.cpp`

## 날짜별 요약

### 2026-04-10

- Decal Component, Actor, Scene Proxy, Shader Constant, Render Pass 등 데칼의 기본 골격이 추가되었습니다.

### 2026-04-11

- Projection Decal 구현이 들어갔고 Fade와 알파 관련 보정이 진행되었습니다.
- Fireball과 Exponential Height Fog가 렌더러에 처음 편입되었습니다.
- 에디터 Spawn / 선택 / Show Flag 연결도 같이 들어갔습니다.

### 2026-04-12

- Decal 수집 구조가 현재 RenderBus 중심 구조로 가까워졌습니다.
- Fireball Additive 패스가 추가되었습니다.
- Tick / Runtime 업데이트 흐름이 수정되었습니다.
- Normal MRT 출력이 추가되었습니다.

### 2026-04-13

- Receiver 설정이 `StaticMeshComponent`로 이동했습니다.
- SAT 기반 데칼 교차 판정과 Decal Stat이 추가되었습니다.
- Billboard scale ignore, projection arrow 등 authoring 품질이 개선되었습니다.
- Fireball 컬링, outline 비지원, falloff 품질이 정리되었습니다.
- EggActor와 충돌 기반 splat 흐름이 추가되었습니다.
- Albedo MRT 출력도 추가되었습니다.

### 2026-04-14

- FXAA가 추가되고 보정되었습니다.
- Scene Depth ViewMode와 Selection Mask가 추가되었습니다.
- Fog 구현 문서가 별도 문서로 정리되었습니다.
- Decal Show Flag와 통계 표시가 확장되었습니다.

### 2026-04-15

- FXAA 설정 UI가 별도 위젯으로 분리되었습니다.
- Fireball additive blending이 정리되었습니다.
- Spotlight Actor가 추가되었습니다.
- Grid 순서 문제와 Gizmo 회전 버그가 수정되었습니다.
- 충돌 노말이 HitResult에 반영되었습니다.

## 결론

`4f014ea`부터 `da19ace`까지의 범위는 단순히 "데칼 하나 추가" 수준이 아니라, 다음 기능들이 함께 자리잡은 시기였습니다.

- Projection Decal과 receiver filtering
- Scene buffer 기반 Fireball 효과
- Screen-space Exponential Height Fog
- 충돌 기반 Egg / Projectile splat
- Spotlight 형태의 복합 액터
- SceneDepth / SelectionMask / FXAA 후처리 경로
- 이를 제어하고 확인하기 위한 에디터/디버그 툴링

즉, 이 구간은 하나의 독립 기능 브랜치라기보다, 렌더링 효과와 에디터 기능이 함께 확장된 "효과 시스템 통합 단계"로 보는 것이 맞습니다.
