# NipsEngine 신규 기능 개발 가이드 (Decal, Fog, Fireball/SubUV)

본 문서는 NipsEngine에 최근 구현된 Decal, Height Fog, Fireball 및 SubUV 시스템의 기술적 상세와 사용법을 설명합니다.

---

## 1. Decal (데칼 시스템)

데칼은 월드 공간의 특정 영역(볼륨) 내에 텍스처를 투영하는 기능을 제공합니다.

### 1.1 개요
- **컴포넌트**: `UDecalComponent` (또는 `ADecalActor`를 통해 배치)
- **렌더 패스**: `ERenderPass::Decal`
- **쉐이더**: `ShaderDecal.hlsl`

### 1.2 주요 기능 및 속성
- **투영 방식**: 0.5 유닛 크기의 큐브 영역 내에서 인버스 월드 매트릭스(`InvDecalWorld`)를 사용하여 로컬 좌표로 변환 후, 영역 밖은 `clip()` 처리합니다.
- **주요 속성**:
  - `DecalSize`: 데칼이 투영될 볼륨의 크기 (기본값: 5.0, 5.0, 5.0).
  - `DecalColor`: 데칼 텍스처와 곱해질 틴트 컬러.
  - **페이드 효과**: `SetFadeIn`, `SetFadeOut` 함수를 통해 생성/소멸 시 부드러운 전환 및 자동 소멸 기능을 지원합니다.
- **머티리얼**: 단일 머티리얼 슬롯을 지원하며, `DiffuseMap`을 통해 텍스처를 투영합니다.

---

## 2. Height Fog (하이트 포그 시스템)

공기 중의 안개 효과를 시뮬레이션하며, 고도에 따른 밀도 변화를 지원합니다.

### 2.1 개요
- **컴포넌트**: `UHeightFogComponent`
- **렌더 패스**: `ERenderPass::Fog` (Post-process 방식)
- **쉐이더**: `FogPass.hlsl`, `HeightFogPixelShader.hlsl`

### 2.2 주요 기능 및 속성
- **멀티 레이어**: 최대 8개(`MAX_FOG_LAYER_COUNT`)의 독립적인 포그 레이어를 지원합니다.
- **주요 속성**:
  - `FogDensity`: 안개의 전반적인 밀도.
  - `HeightFalloff`: 고도가 높아짐에 따라 안개가 옅어지는 정도 (지수 함수 기반).
  - `FogHeight`: 안개의 기준 고도.
  - `FogInscatteringColor`: 안개의 색상.
  - `FogStartDistance` / `FogCutoffDistance`: 안개가 시작되는 거리와 완전히 사라지는 거리.
  - `FogMaxOpacity`: 안개의 최대 불투명도 제한.
- **렌더링 원리**: G-Buffer의 Depth와 WorldPosition을 사용하여 카메라로부터의 거리 및 월드 Z 좌표를 계산, 지수 함수 기반 투과율(Transmittance)을 산출하여 최종 색상에 블렌딩합니다.

---

## 3. Fireball & SubUV (파티클 및 스프라이트 시스템)

### 3.1 Fireball (파이어볼)
파이어볼은 태양과 같은 발광체를 표현하기 위한 복합 액터입니다.
- **구성**:
  - `StaticMesh`: 구체(Sphere) 형태의 메시 (`Asset/Mesh/Sun/sun.obj`).
  - `UFireballComponent`: 광원 정보를 담는 컴포넌트로, `RenderCollector`에서 감지되어 라이팅 계산에 활용됩니다.
- **주요 속성**: `Intensity`, `Radius`, `RadiusFallOff`, `Color`.

### 3.2 SubUV (애니메이션 스프라이트)
텍스처 아틀라스를 사용한 애니메이션 스프라이트 렌더링 시스템입니다.
- **컴포넌트**: `USubUVComponent` (또는 `ASubUVActor`)
- **렌더 패스**: `ERenderPass::SubUV`
- **배처**: `FSubUVBatcher`를 통한 효율적인 일괄 렌더링.
- **쉐이더**: `ShaderSubUV.hlsl`
- **주요 기능**:
  - **아틀라스 지원**: `Columns` x `Rows` 그리드 기반 텍스처 아틀라스 지원.
  - **재생 제어**: `PlayRate`(FPS), `Loop`(반복 여부) 설정 및 실시간 프레임 인덱스 업데이트.
  - **빌보딩**: `UBillboardComponent`를 상속받아 항상 카메라를 향하도록 설계되었습니다.
- **렌더링 최적화**: `FSubUVBatcher`가 동일한 텍스처를 사용하는 스프라이트들을 정렬하여 드로우콜을 최소화합니다.

---

## 4. 엔진 통합 및 사용법 (Editor)

- **배치 방법**: Editor의 Viewport Overlay 또는 Control Widget에서 "Decal", "Fireball", "SubUV" 등을 선택하여 즉시 월드에 스폰할 수 있습니다.
- **속성 편집**: `PropertyWidget`에서 각 컴포넌트의 파라미터(밀도, 크기, 색상 등)를 실시간으로 조정할 수 있습니다.
- **디버깅**: `EditorViewportOverlayWidget`에서 데칼의 통계(총 개수, 컬렉션 타임 등)를 확인할 수 있으며, 체크박스를 통해 표시 여부를 전환할 수 있습니다.
