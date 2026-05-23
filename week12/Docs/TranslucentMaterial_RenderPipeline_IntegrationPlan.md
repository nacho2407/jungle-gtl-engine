# Translucent Material Render Pipeline Integration Plan

## 1. 목적

반투명 재질 렌더링을 목표 기능으로 고정하고, 기존에 보류되었던 렌더 파이프라인 개편 계획 중 현재 작업에 직접 도움이 되는 항목만 선별해 통합한다.

이번 계획의 핵심은 전체 렌더러를 한 번에 registry/runner 구조로 갈아엎는 것이 아니라, 재질 특성에 따라 render queue, render pass, shader output, renderer state가 일관되게 결정되도록 정리하는 것이다.

## 2. 통합 범위

### 필수 수행 항목

- 현재 pass 순서, render target 흐름, culling 상태, editor/picking 영향 범위 기준선 정리
- 현재 소스에 이미 들어온 `FTranslucentRenderPass` 초기 구현을 기준선으로 검토하고, 계획과 다른 부분을 선행 정리
  - 현재 pass 순서가 계획과 다르면 `Fog -> Translucent -> SubUV -> Sandevistan -> PostProcess -> FXAA` 순서로 재정렬한다.
  - 현재 `FTranslucentRenderPass`가 기존 material PS와 `UberLit` macro 경로를 그대로 쓰는 부분은 전용 shader 도입 작업에서 교체한다.
- `MaterialBlendMode` 도입
- material-authored blend state descriptor 도입
- material 기반 render queue/pass 라우팅
- translucent command 정렬
- opaque/translucent shader output 정책 분리
  - 현재 작업에서는 opaque `UberLit`의 기존 MRT 출력 계약을 유지한다.
  - translucent shader만 color-only 출력으로 분리한다.
- translucent pass의 depth/blend state 정리
- `.mat` 저장/로드 및 editor authoring 경로 정리
- 현재 단계에서는 `Masked`를 구현 범위에서 제외하고, `Opaque`/`Translucent`만 기능 대상으로 삼는다.

### 보류 계획에서 흡수할 항목

- Phase 0 기준선 고정은 전체 흡수
- Phase 1 pass registry/runner는 전면 도입하지 않고, pass 순서와 pass 의존성 문서화만 흡수
- Phase 3 view mode/pass gating 중앙화는 최소 helper 또는 정책 정리 수준으로 흡수
- Phase 4 draw submission/execution 분리는 full `FDrawCommandList` 대신 mesh draw 공통화 수준으로만 흡수

### 명시적 보류

- `Masked` material mode와 alpha-test depth/shadow path
- opaque `UberLit` color-only 전환과 editor/debug viewmode shader 분리
  - 별도 문서: `Docs/UberLit_ColorOnly_ViewModeShader_SeparationPlan.md`
- decal rendering의 `UberLit` 분리
  - 현재 작업에서는 기존 `UberLit` 내부 decal 합성 경로를 유지한다.
  - 별도 `DecalRenderPass` 복구/등록과 `Decal.hlsl` 정리는 opaque `UberLit` 후속 리팩토링으로 분리한다.
- pass registry/runner 전면 도입
- present, screen overlay, editor picking의 root pipeline 편입
- full `FDrawCommandList` 도입
- resource/context 소유권 전면 재편
- render graph 도입
- W10 pipeline 구조 전면 이식

## 3. Phase 계획

### Phase 0. 기준선 및 위험 구간 고정

목표: 반투명 도입 전, 현재 렌더러의 의존성과 깨지기 쉬운 지점을 먼저 고정한다.

작업:

- runtime/editor/viewer 렌더 호출 경로 확인
- 현재 pass 순서 정리
  - 확정 순서: `DepthPre -> LightCulling -> Shadow -> VSMConversion -> Opaque -> ViewModeResolve -> Fog -> Translucent -> SubUV -> Sandevistan -> PostProcess -> FXAA -> Font -> SelectionMask -> Grid/Editor/EditorOverlay -> DepthLess -> PostProcessOutline`
  - 현재 소스에서 `Translucent`/`SubUV`가 `FXAA`/`Font` 뒤에 있다면, 이 phase에서 먼저 계획 순서로 이동한다.
  - `SubUV`는 scene effect로 취급하므로 `Translucent` 직후에 합성한다.
  - `Font`는 UI text 성격을 유지하므로 `FXAA` 이후에 둔다.
  - pass 순서를 바꿀 때 `GetRenderPassPerfName()` 또는 동등한 GPU timing/stat 이름 배열도 같은 순서로 갱신한다.
- `PrevPassSRV/RTV`, `SceneFinalSRV`, 주요 render target 흐름 정리
- frustum/culling 관련 막힌 동작 확인
- static/skeletal opaque 렌더링 회귀 기준 확인
- selection, picking, editor overlay가 참조하는 pass queue 정리
- `DepthPre`/`Shadow` 입력 queue 정책 고정
  - `DepthPre = Opaque`
  - `Shadow = Opaque`
  - `Translucent = DepthPre/Shadow 제외`
- `LightCulling` 최종 정책을 고정한다.
  - `LightCulling` 생성 입력은 기존처럼 `DepthPre`가 만든 opaque depth SRV만 사용한다.
  - `Translucent`는 depth write를 하지 않으므로 `LightCulling` 생성 입력에는 포함하지 않는다.
  - `Translucent` shading은 opaque depth 기반으로 생성된 동일 light culling 결과를 소비한다.
- decal 현재 경로와 보류 정책을 고정한다.
  - 현재 `DecalRenderPass`는 pass list에 없고, `ERenderPass::Decal` command는 `UpdateUberBuffer()`를 통해 `UberLit` 내부 decal 합성 데이터로 사용된다.
  - 현재 단계에서는 이 경로를 유지하고, dedicated decal pass 재도입은 이번 translucent 작업 범위에서 제외한다.
  - decal recipient 정책은 현행 static mesh 대상만 기준선으로 기록하고, skeletal/procedural 확장은 후속 작업으로 둔다.
- 현재 반투명 관련 선반영 코드 정리 범위를 고정한다.
  - `FTranslucentRenderPass` 파일이 이미 존재하면 삭제하지 않고, pass 순서와 shader/state 정책을 계획에 맞게 보정한다.
  - `FTranslucentRenderPass`의 draw loop가 기존 `UberLit` MRT shader를 그대로 호출하는 상태라면 Phase 3에서 `UberTranslucent.hlsl` 호출로 교체한다.
  - 기존 `ERenderPass::Translucent` enum과 id-pick 참조는 유지하되, 실제 command 유입 정책은 Phase 2의 material routing에서 확정한다.

완료 기준:

- 코드 변경 없이 반투명 추가 시 영향받는 지점과 검증 기준이 정리된다.
- `DepthPre`/`Shadow`/`LightCulling`/`Translucent` 사이의 queue, depth, SRV 의존성이 확정된다.
- 기존 선반영 코드 중 유지할 것과 교체할 것이 구분된다.

우선순위: P0

### Phase 1. Material 의미 계층 정리

목표: D3D state가 아니라 재질 의미로 렌더링 경로를 결정할 수 있게 만든다.

작업:

- `MaterialBlendMode` 도입
  - `Opaque`
  - `Translucent`
- `MaterialBlendMode`는 기존 `EMaterialShaderType`과 분리한다.
  - `EMaterialShaderType`은 어떤 shader family를 쓸지 나타낸다.
  - `MaterialBlendMode`는 render queue, pass, depth/write policy를 결정한다.
  - `SurfaceLit + Opaque`는 기존 `UberLit` 경로를 유지한다.
  - `SurfaceLit + Translucent`는 `Translucent` queue와 `UberTranslucent.hlsl` color-only PS 경로를 사용한다.
- `Masked`는 현재 단계에서 제외한다.
  - alpha clip, alpha-test depth prepass, masked shadow map은 이번 작업 범위가 아니다.
  - 추후 `Masked`를 도입할 때는 `DepthPre`/`Shadow`에 alpha-test shader variant가 필요하다.
- material이 직접 지정할 수 있는 blend state descriptor 도입
  - blend enable
  - source/destination color blend factor
  - color blend op
  - source/destination alpha blend factor
  - alpha blend op
  - render target write mask
- blend option은 D3D11 blend factor와 같은 의미 구조를 사용한다.
  - 예: `Zero`, `One`, `SrcColor`, `InvSrcColor`, `SrcAlpha`, `InvSrcAlpha`, `DestAlpha`, `InvDestAlpha`, `DestColor`, `InvDestColor`.
  - C++ enum 이름은 `EBlendOption` 또는 동등한 이름으로 둔다.
  - blend op도 D3D11 blend op와 같은 의미 구조를 사용한다.
  - 예: `Add`, `Subtract`, `RevSubtract`, `Min`, `Max`.
  - C++ enum 이름은 `EBlendOp` 또는 동등한 이름으로 둔다.
  - `FMaterialBlendStateDesc`는 `SrcColor`, `DestColor`, `ColorOp`, `SrcAlpha`, `DestAlpha`, `AlphaOp`, `WriteMask`처럼 source/destination 역할이 드러나는 필드명을 사용한다.
  - JSON 저장 시에는 가독성을 위해 `"One"`, `"Zero"`, `"SrcColor"`, `"Add"` 같은 문자열을 사용한다.
  - string <-> enum 변환 helper는 deserialize fallback과 editor UI가 공유하도록 한 곳에 둔다.
- `AlphaBlend`, `Additive`, `Multiply` 등은 enum 고정값이 아니라 blend state descriptor preset으로 제공
- `EBlendType::AlphaBlend` 등 기존 enum 기반 blend state는 당분간 compatibility preset으로 유지
- 최종 단계에서는 `EBlendType` 기반 material blend 지정 경로를 삭제
- 구체 구현 단위
  - `Material.h`에 `EMaterialBlendMode`와 `FMaterialBlendStateDesc`를 추가한다.
  - `UMaterialInterface`에 blend mode와 blend state descriptor 조회 API를 추가한다.
  - `UMaterial`은 기본 blend mode/descriptor 값을 보유한다.
  - `UMaterialInstance`도 blend mode/descriptor override를 지원한다.
  - instance에 override 값이 없으면 parent 값을 따른다.
  - instance에 override 값이 있으면 해당 값이 render queue와 blend state cache key에 반영된다.
  - 기존 `BlendType`/`DepthStencilType`은 전환 기간 동안 runtime/editor 보조 경로로 유지하되, surface mesh routing의 기준으로 쓰지 않는다.
  - `FRenderStateResourceCache` 또는 동등한 cache에 descriptor 기반 `ID3D11BlendState` 생성/재사용 경로를 추가한다.
- `MaterialBlendMode -> RenderQueue/Pass/DepthState` 변환 정책 중앙화
  - `Opaque -> ERenderPass::Opaque, DepthWrite On`
  - `Translucent -> ERenderPass::Translucent, DepthReadOnly`
- `BlendStateDesc -> ID3D11BlendState` 생성/캐싱 경로 확장
- `Opacity`, diffuse alpha 역할 분리
- `.mat` serialize/deserialize에 blend mode와 blend state descriptor 포함
  - `BlendMode`는 `"Opaque"` / `"Translucent"` 문자열로 저장한다.
  - `BlendState`는 object로 저장하고, 내부 option/op 값은 문자열로 저장한다.
  - 기존 `.mat`에 blend mode가 없으면 `Opaque`로 로드한다.
  - 기존 `.mat`에 blend descriptor가 없으면 blend mode에 맞는 preset을 적용한다.
  - `Translucent` 기본 preset은 `SrcAlpha / InvSrcAlpha / Add`, alpha는 `One / Zero / Add`, write mask는 RGBA로 둔다.
  - 기존 파일 로드만으로는 자동 schema rewrite를 수행하지 않는다. 저장 시점에만 새 schema를 기록한다.
- 기존 material 기본값은 `Opaque`로 유지
- 기존 material asset과의 장기 호환성은 보장하지 않음
- FBX/import opacity 정책
  - FBX나 기존 material load 경로에서 `Opacity < 1`이 들어와도 자동으로 `Translucent`로 전환하지 않는다.
  - blend mode 전환은 editor authoring에서 명시적으로 수행한다.
  - import 결과 material은 기본적으로 `Opaque`이며, 테스트용 translucent material도 editor나 명시적 asset 편집으로 만든다.
- editor Details 노출 경로 정리
  - `Blend Mode` 선택 UI는 `Opaque`/`Translucent`만 노출한다.
  - `Blend Preset`은 `AlphaBlend`, `Additive`, `Multiply`를 descriptor preset으로 적용한다.
  - 고급 descriptor 편집은 필요할 때만 펼침 UI로 노출한다.
  - base material과 material instance 모두에서 blend mode/descriptor를 편집할 수 있다.
  - material instance UI는 parent 값을 보여주되, override 활성화 여부를 명확히 분리한다.

완료 기준:

- material asset만 보고 opaque/translucent 판단이 가능하다.
- material이 가진 blend state descriptor를 기준으로 공유 `ID3D11BlendState`를 생성/재사용할 수 있다.
- `Masked`가 이번 단계의 material authoring UI와 serialize 정책에 섞이지 않는다.

우선순위: P1

### Phase 2. RenderQueue 및 Command 라우팅

목표: mesh command가 primitive type이 아니라 material policy에 따라 pass로 들어가게 만든다.

작업:

- static mesh command 라우팅을 material 기준으로 변경
- skeletal mesh command 라우팅을 material 기준으로 변경
- procedural mesh command 라우팅을 material 기준으로 변경
- `ResolveMaterialRenderPass()` 또는 동등한 중앙 함수 추가
  - `ResolveMaterialRenderPass(Material)`은 material이 없으면 `Opaque`를 반환한다.
  - `ResolveMaterialDepthPolicy(Material)` 또는 동등한 helper로 translucent draw 시 `DepthReadOnly`를 강제한다.
  - static/skeletal/procedural mesh builder는 `RenderBus.AddCommand(ERenderPass::Opaque, Cmd)`를 직접 박지 않고 helper 결과를 사용한다.
- translucent queue back-to-front 정렬 추가
  - 우선 구현은 가장 쉬운 방식으로 진행한다.
  - `FTranslucentRenderPass` draw 직전에 command를 복사한 뒤 back-to-front로 정렬한다.
  - 정렬 로직은 `SortTranslucentCommands(...)` 또는 동등한 별도 함수로 분리한다.
  - 추후 정렬 기준을 교체하기 쉽도록 sort key 계산도 별도 helper로 둔다.
  - 같은 material/mesh batching 이득보다 올바른 blending 순서를 우선한다.
- render collect 단계에서 translucent command의 depth state를 `DepthReadOnly`로 강제 덮어쓴다.
  - material asset이 실수로 `Default` depth state를 들고 있어도 translucent queue에 들어가는 순간 pass 정책이 우선한다.
  - pass 내 binding은 collect 단계에서 이미 일관된 값으로 보정되었다고 가정한다.
  - `Translucent + DepthWrite`처럼 모순되는 상태는 collect 단계에서 `Translucent + DepthReadOnly`로 덮어쓴다.
  - translucent pass는 material/command의 확정 값을 바인딩하고, 별도 정책 재해석은 하지 않는다.
- shadow, selection mask, id pick이 어느 queue를 참조할지 정책 결정
  - shadow는 `Opaque`만 참조한다.
  - id pick은 `Opaque`, `Translucent`, `SubUV`를 참조한다.
  - selection mask/outline은 translucent 선택 가능 여부를 유지하도록 `Translucent` queue도 참조한다.
- selection mask 구체 작업
  - 현재 selected primitive의 mask command를 만들 때 `Opaque` queue만 복사하는 경로를 `Opaque + Translucent` queue 대상으로 확장한다.
  - `Translucent` material이라도 selection mask pass에서는 선택 윤곽이 보이도록 `SelectionMask` 전용 shader/state를 사용한다.
  - static/skeletal/procedural surface mesh를 모두 selection mask 대상으로 포함한다.
  - procedural mesh가 기존 selection shader key에 없다면 static mesh selection path를 재사용하거나 별도 procedural key를 추가한다.
  - `SubUV`/billboard selection mask 기존 경로는 유지하고, surface mesh 확장과 섞지 않는다.
- translucent command는 depth write를 하지 않으므로 back-to-front 정렬 기준은 camera distance와 command bounds를 사용한다.

완료 기준:

- translucent material을 가진 mesh command가 `Translucent` pass로 들어간다.
- 기존 opaque 렌더링은 유지된다.
- `DepthPre`와 `Shadow` command source가 opaque-only로 유지된다.

우선순위: P1

### Phase 3. Shader Output 및 Translucent Pass 정리

목표: 실제 반투명 색이 올바르게 합성되게 만든다.

작업:

- translucent 전용 `UberTranslucent.hlsl`을 추가한다.
  - 이번 계획의 확정 파일명은 `Shaders/Material/UberTranslucent.hlsl`로 둔다.
  - VS는 기존 vertex factory/base pass entry를 공유하고, PS만 translucent 전용으로 분리하는 구성을 우선 검토한다.
- shader path/type 구성 추가
  - `FShaderPaths`에 `MaterialUberTranslucent = "Shaders/Material/UberTranslucent.hlsl"`를 추가한다.
  - `MaterialShaderTypes`에는 별도 shader family enum을 늘리기보다, 우선 `MaterialBlendMode::Translucent`일 때 pass에서 PS path를 override하는 구성을 우선 적용한다.
  - 장기적으로 material authoring에서 shader family를 분리해야 할 필요가 생기면 `SurfaceTranslucent` 타입 도입을 후속 검토한다.
- `FTranslucentRenderPass` shader lookup 변경
  - VS key는 기존 vertex factory desc의 base-pass VS를 그대로 사용한다.
  - PS key는 material의 `SurfaceLit` PS path가 아니라 `FShaderPaths::MaterialUberTranslucent`와 `mainPS`를 사용한다.
  - permutation macro는 기존 lighting/material feature macro를 재사용하되, MRT 전용 output 전제는 제거한다.
- translucent PS는 color-only output을 사용한다.
  - `Opacity`와 diffuse texture alpha를 최종 alpha에 반영한다.
  - Normal/WorldPos MRT는 기록하지 않는다.
  - `PSOutput`은 단일 `float4 Color : SV_TARGET0` 또는 직접 `float4 mainPS(...) : SV_TARGET0` 형태로 둔다.
  - diffuse texture alpha가 없으면 material `Opacity`만 사용한다.
  - `clip()` 기반 alpha-test는 `Masked` 범위이므로 이번 shader에서는 사용하지 않는다.
- translucent shader material cbuffer 계약
  - 기존 material parameter reflection 흐름을 유지하기 위해 `UberTranslucent.hlsl`도 material cbuffer slot `b2`를 사용한다.
  - 최소 필드는 `AmbientColor`, `DiffuseColor`, `SpecularColor`, `Shininess`, `ScrollUV`, `EmissiveColor`, `Opacity`로 둔다.
  - `Opacity`는 최종 alpha에 곱한다.
  - 최종 alpha 계산은 `saturate(Opacity * DiffuseTex.a)`를 기본으로 한다.
  - diffuse texture가 없으면 `DiffuseTex.a = 1.0`으로 처리한다.
  - receive decal은 지원하지 않는다.
  - `DecalCount`, `Decals`, `DecalDiffuseTexture`를 translucent shader에서 참조하지 않는다.
- opaque `UberLit`의 기존 MRT 출력 계약은 이번 작업에서 유지한다.
  - opaque color-only 전환과 editor/debug viewmode shader 분리는 별도 후속 작업으로 분리한다.
  - 관련 의도와 전제는 `Docs/UberLit_ColorOnly_ViewModeShader_SeparationPlan.md`에 기록한다.
  - `UberLit` 내부 decal 합성 루프도 이번 작업에서 제거하지 않는다.
  - decal 분리는 translucent shader 분리와 독립된 후속 리팩토링으로 관리한다.
- translucent depth는 read-only 적용
- translucent blend는 material의 blend state descriptor를 사용
  - blend state는 material descriptor cache에서 얻되, 값이 없으면 `AlphaBlend` preset을 사용한다.
- translucent pass 위치는 `Fog` 이후, `SubUV` 이전으로 확정한다.
  - translucent는 fog 결과 위에 합성된다.
  - `SubUV`는 scene effect이므로 translucent 직후에 합성한다.
  - `SubUV`는 alpha blend와 depth test를 사용하되 depth write는 하지 않는 정책으로 맞춘다.
  - 기존 `SubUVBatcher` material이 `Default` depth state를 사용한다면 `DepthReadOnly`로 보정한다.
  - camera effect, vignette/gamma, FXAA는 translucent와 SubUV까지 포함한 최종 color에 적용된다.
- LightCulling 결과는 translucent shader에서도 동일하게 참조한다.
  - 현재 light culling은 `DepthPre`가 만든 opaque depth SRV를 입력으로 tile/cluster light list를 만든다.
  - translucent는 depth write를 하지 않으므로 light culling 입력에는 들어가지 않는다.
  - translucent pixel shading은 같은 camera/tile/cluster 좌표로 culled light list를 읽어 조명 계산에 사용한다.
  - 이 정책은 현재 단계의 최종 결정으로 고정하며, translucent를 별도 light culling 입력으로 확장하는 작업은 이번 범위에 포함하지 않는다.

완료 기준:

- static/skeletal translucent material이 depth test와 alpha blend를 정상 수행한다.
- `UberTranslucent.hlsl`이 새 파일로 존재하고, translucent pass가 해당 PS를 사용한다.
- translucent material이 opaque와 같은 tiled/clustered light culling 결과를 사용한다.
- opaque `UberLit` MRT 계약과 기존 editor/debug viewmode 기능이 이번 작업 때문에 퇴행하지 않는다.

우선순위: P1

### Phase 4. Mesh Draw 공통화

목표: opaque/translucent pass가 같은 mesh draw 기반을 공유하도록 중복을 줄인다.

작업:

- vertex factory binding 공통화
- material binding 공통화
- per-object constant buffer update 공통화
- shader lookup/permutation key 생성 일부 공통화
  - VS key와 vertex layout 선택은 opaque/translucent가 공유한다.
  - PS key와 render target output policy는 opaque/translucent가 분리한다.
- pass별 차이는 target, queue, state policy, sort policy로 제한
- full `FDrawCommandList` 도입은 하지 않음

완료 기준:

- opaque/translucent draw loop 중복이 줄고, material/render state 변경 시 수정 지점이 감소한다.

우선순위: P2

### Phase 5. ViewMode 및 Pass Gating 최소 정리

목표: view mode와 pass skip 조건이 반투명 경로와 충돌하지 않게 만든다.

작업:

- `Wireframe`, `Depth`, `Normal`, `Heatmap`, `BoneWeightHeatmap`에서 translucent 처리 정책 결정
- 이번 작업에서는 view mode별 translucent/debug 최종 정책을 확정하지 않고 후속 `Docs/UberLit_ColorOnly_ViewModeShader_SeparationPlan.md`와 함께 다룬다.
- 다만 장기 방향은 `Translucent`/`SubUV` 합성 이후 debug/viewmode 출력이 수행될 수 있도록 정리하는 것이다.
- opaque MRT 의존성 제거 자체는 이번 작업 범위가 아니며, 별도 후속 계획서의 검토 대상으로 둔다.
- `SetSkipWireframe` 계열 임시 정책 점검
- 필요한 경우 작은 helper 수준으로 pass gating 정책 중앙화
- full `FViewModePassRegistry`는 보류

완료 기준:

- 주요 view mode 전환 시 opaque/translucent/editor pass가 의도대로 동작한다.

우선순위: P2

### Phase 6. 검증 및 정리

목표: 기능 회귀와 남은 구조 부채를 분리해서 마무리한다.

작업:

- `ReleaseBuild.bat`
- opaque static/skeletal 기존 장면 확인
- translucent static/skeletal 테스트
- translucent procedural mesh 테스트
- 겹친 translucent mesh 정렬 확인
- translucent가 opaque depth 뒤에서 정상 depth test되고, depth write를 하지 않는지 확인
- tiled/clustered light culling 모드에서 translucent 조명 적용 확인
- editor selection/picking 확인
  - id pick은 `Opaque`, `Translucent`, `SubUV` 대상 모두 확인한다.
  - selection mask/outline은 `Opaque`와 `Translucent` surface mesh 모두 확인한다.
  - static/skeletal/procedural translucent mesh selection을 각각 확인한다.
- 검증 fixture
  - opaque mesh 뒤에 translucent static mesh를 겹쳐 depth test와 alpha blend를 확인한다.
  - translucent skeletal mesh 또는 skeletal mesh section으로 GPU/CPU skinning 경로를 확인한다.
  - translucent procedural mesh로 routing과 selection mask를 확인한다.
  - 서로 겹친 translucent mesh 2개 이상으로 back-to-front 정렬을 확인한다.
  - selected translucent object에서 id pick과 outline/selection mask를 확인한다.
  - SubUV effect가 translucent 직후, Sandevistan/PostProcess/FXAA 이전에 합성되는지 확인한다.
- shadow/culling 영향 확인
- 기존 `UberLit` 내부 decal 합성 경로 회귀 확인
- `Normal`/`WorldPos`/`Depth`/`Heatmap`/`BoneWeightHeatmap` view mode 회귀 확인
- 남은 보류 항목 문서화

완료 기준:

- 반투명 재질 렌더링이 기본 렌더 경로에서 동작한다.
- 기존 opaque/static/skeletal/editor 경로의 주요 회귀가 없다.
- 장기 보류 항목이 현재 작업 범위와 분리되어 있다.

우선순위: P0/P1 검증

## 4. 권장 작업 순서

1. Phase 0: 기준선 및 위험 구간 고정
2. Phase 1: Material 의미 계층 정리
3. Phase 2: RenderQueue 및 Command 라우팅
4. Phase 3: Shader Output 및 Translucent Pass 정리
5. Phase 4: Mesh Draw 공통화
6. Phase 5: ViewMode 및 Pass Gating 최소 정리
7. Phase 6: 검증 및 정리

단, Phase 4의 mesh draw 공통화는 작업량이 커질 경우 Phase 3 이후로 미룬다. 우선은 반투명 렌더링의 기능 검증을 끝낸 뒤 중복 제거를 진행하는 것이 안전하다.

## 5. 구조적 메리트 판단

| 항목 | 통합 수준 | 구조적 메리트 | 비고 |
|---|---|---:|---|
| 기준선 고정 | 전체 흡수 | 5/5 | culling, pass 순서, target 흐름 확인에 필수 |
| pass registry/runner | 보류, 일부 문서화만 흡수 | 4/5 | 이득은 크지만 현재 기능 추가 범위보다 큼 |
| view mode/pass gating | 최소 흡수 | 4/5 | 반투명과 wireframe/view mode 충돌 방지에 유효 |
| draw submission/execution 분리 | mesh draw 공통화로 축소 | 5/5 | 장기 메리트는 크지만 full 도입은 과함 |
| resource/context 재편 | 보류 | 4/5 | target/lifetime 전체 리팩토링으로 번질 가능성이 큼 |

## 6. 현재 단계의 원칙

- 새 기능보다 먼저 material policy와 render queue 경계를 정리한다.
- 현재 소스에 이미 들어온 `FTranslucentRenderPass`는 유지하되, pass 순서와 shader/state 정책을 계획에 맞게 보정한다.
- `ERenderPass`는 당장 유지하되, material이 pass를 직접 알지 않게 한다.
- material은 blend state descriptor를 직접 가질 수 있다.
- `EBlendType` 기반 preset은 전환 기간에만 유지하고 최종 단계에서 제거한다.
- translucent는 먼저 기본 alpha blend 기능을 안정화하고, fog/shadow 품질은 후속 정책으로 분리한다.
- `Masked`는 현재 작업에서 제외한다. 이번 단계의 `DepthPre`/`Shadow`는 `Opaque` 전용으로 유지한다.
- translucent는 `DepthPre`/`Shadow`/`LightCulling` 생성 입력이 아니지만, opaque depth 기반 `LightCulling` 결과는 shading에서 재사용한다.
- translucent는 `Fog` 이후, `Sandevistan`/`PostProcess`/`FXAA` 이전에 합성한다.
- `SubUV`는 scene effect로 보고 `Translucent` 직후, `Sandevistan`/`PostProcess`/`FXAA` 이전에 합성한다.
- translucent shader는 `UberLit` MRT 경로에 얹지 않고 새 `Shaders/Material/UberTranslucent.hlsl` color-only PS로 분리한다.
- translucent shader는 receive decal을 지원하지 않는다.
- opaque `UberLit` color-only 전환과 editor/debug view mode shader 분리는 이번 작업의 필수 범위가 아니며 별도 계획서로 관리한다.
- decal은 현재 단계에서 `UberLit` 내부 합성 경로를 유지하고, `DecalRenderPass` 복구/분리는 opaque `UberLit` 후속 리팩토링으로 분리한다.
- editor/picking/selection 경로는 opaque 전용 가정이 있는지 반드시 확인하고, selection mask는 `Opaque + Translucent` static/skeletal/procedural surface mesh를 대상으로 한다.
- W10식 전체 pipeline migration은 현재 작업의 필수 범위가 아니다.
