# UberLit Color-Only and ViewMode Shader Separation Plan

## 1. 목적

현재 `UberLit`은 기본 lit rendering과 editor/debug view mode 출력을 한 shader 경로에 함께 담고, `Color`, `Normal`, `WorldPos` MRT를 동시에 기록한다.

이 구조는 반투명 material 도입 과정에서 color-only translucent shader와 충돌하지는 않지만, 장기적으로 opaque 기본 렌더링까지 color-only로 단순화하려면 별도 정리가 필요하다.

이 문서는 해당 정리를 현재 translucent material 작업과 분리된 후속 리팩토링 후보로 기록한다.

## 2. 의도

- 기본 opaque lit pass는 최종 color output에 집중한다.
- `Normal`, `Depth`, `Heatmap`, `BoneWeightHeatmap` 같은 editor/debug view mode는 기본 `UberLit` 출력 계약에 얹지 않는다.
- VS와 vertex factory binding은 최대한 공유하되, PS와 render target policy는 목적별로 분리한다.
- `SceneNormal`/`SceneWorldPos` 같은 중간 render target은 실제 소비자가 남아 있을 때만 유지한다.

## 3. 목표

- `UberLit`의 기본 surface shading path를 단일 color output으로 축소할 수 있는 구조를 만든다.
- debug/viewmode 출력은 별도 shader 또는 별도 PS entry로 분리한다.
- `Normal` view mode는 `SceneNormal` MRT 의존 대신 debug PS가 직접 normal color를 출력하는 방향을 검토한다.
- `Depth` view mode는 기존 depth buffer 기반 fullscreen resolve를 유지할 수 있는지 검토한다.
- fog가 `SceneWorldPos`에 직접 의존하지 않도록 depth reconstruction 기반으로 전환 가능한지 검토한다.
- translucent material 작업 중 확정되는 pass 순서, render target 흐름, shader binding 정책을 후속 리팩토링의 입력 전제로 삼는다.
- decal 합성은 현재 `UberLit` 내부 루프를 단기 유지하되, 후속 리팩토링에서는 dedicated decal pass 또는 별도 surface-effect 경로로 분리할지 결정한다.

## 4. 현재 전제

- translucent material 작업에서는 `UberTranslucent.hlsl` 또는 동등한 별도 translucent shader를 먼저 color-only로 도입한다.
- opaque `UberLit` MRT 제거는 translucent 기능 안정화 이후 별도 작업으로 다룬다.
- 현재 fog는 `SceneWorldPos`를 사용하므로, `SceneWorldPos` 제거 전에는 fog의 world position 복원 정책을 먼저 정해야 한다.
- 현재 `Normal` view mode는 `SceneNormal`을 사용하므로, `SceneNormal` 제거 전에는 viewmode shader 분리 정책을 먼저 정해야 한다.
- 현재 decal은 `DecalRenderPass`가 아니라 `UpdateUberBuffer()`가 준비한 decal buffer/texture를 `UberLit`이 opaque shading 중 합성하는 경로가 실제 동작 경로다.
- translucent material 본 작업에서는 이 decal 경로를 변경하지 않는다.
- 본 전제는 translucent material 본 작업 또는 뒷마무리 중 변경될 수 있다.

## 5. 비범위

- 지금 문서에서는 구현 phase를 확정하지 않는다.
- render graph 도입, full pipeline registry/runner 전환, 전체 render target 소유권 재편은 포함하지 않는다.
- translucent material 본 작업의 필수 완료 조건으로 삼지 않는다.
- `DecalRenderPass` 복구/등록, `Decal.hlsl` 정리, skeletal/procedural decal recipient 확장은 포함하지 않는다.

## 6. 후속 검토 질문

- fog의 world position 복원은 `InvViewProjection`을 frame constant에 추가하는 방식이 적절한가?
- debug/viewmode shader는 별도 HLSL 파일로 둘지, 기존 vertex shader와 PS entry만 분리할지?
- `Heatmap`/`BoneWeightHeatmap`은 기본 lit shader permutation에서 분리해도 현재 skeletal viewer UX가 유지되는가?
- `UberLit`에서 decal 합성 루프를 제거할 때 dedicated `DecalRenderPass`로 복구할지, 별도 surface-effect shader path로 둘지?
- `SceneNormal`/`SceneWorldPos` render target을 완전히 제거할지, editor/debug 전용 lazy target으로 남길지?
