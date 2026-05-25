# UE Cascade 남은 차이점

기준: `feat/ParticleEmitterInstances` 브랜치의 `6bf8af4e fix: align cascade ribbon tick render flow` 이후 상태.

현재 포팅 목표는 Mesh, Beam2, Ribbon의 UE Cascade CPU Tick / replay 동작을 맞추고, Jungle의 D3D11 CPU staging 렌더 어댑터는 유지하는 것이다. 아래 항목들은 현재 엔진에 대응 기반이 없거나 이번 범위에서 제외한 UE 시스템이라서 stub 또는 어댑터 차이로 남겨둔 부분이다.

## 공통 렌더 경계

- UE의 `FParticleBeamTrailVertexFactory`, `FMeshBatch`, RenderThread, async RHI buffer fill, PSO/resource lifetime 구조는 포팅하지 않았다.
- Jungle은 아래 렌더 어댑터 경계를 유지한다.
  - `EmitterInstance`
  - `FillReplayData`
  - `FDynamic*EmitterData::BuildMeshData`
  - CPU-side `Vertices` / `Indices`
  - `ParticleSystemSceneProxy`
  - D3D11 dynamic VB/IB
- Beam과 Ribbon은 어댑터 이전의 UE replay/payload 의미를 유지한다. 다만 최종 index 출력은 UE의 triangle strip / degenerate stream이 아니라 Jungle 렌더러용 triangle list다.

## Mesh

- `PSA_Velocity`의 orbit-affects-velocity alignment 경로는 stub이다.
  - UE 원본 역할: `RequiredModule->bOrbitModuleAffectsVelocityAlignment`가 켜져 있으면 orbit payload를 반영한 현재/이전 위치로 mesh velocity alignment를 계산한다.
  - Jungle에 없는 기반: RequiredModule의 orbit alignment flag, highest LOD orbit module offset lookup.
  - 나중에 연결할 시스템: RequiredModule orbit flag, orbit payload lookup, UE와 동일한 velocity basis 계산.
- StaticMesh LOD 선택은 현재 LOD0 고정이다.
  - UE 원본 역할: `FDynamicMeshEmitterData::Init`에서 `bUseStaticMeshLODs`, `LODSizeScale`, view-dependent size로 mesh LOD를 고른다.
  - Jungle에 없는 기반: view 기반 mesh particle LOD size 계산.
  - 나중에 연결할 시스템: mesh replay/render adapter 경계에서 StaticMesh LOD selector.
- camera-facing / axis-lock 렌더 orientation은 replay data에는 의미를 보존하지만 실제 렌더 변환은 일부만 반영되어 있다.
  - UE 원본 역할: view/camera basis와 axis lock이 최종 mesh particle transform에 반영된다.
  - Jungle에 없는 기반: UE식 mesh particle view basis 계산.
  - 나중에 연결할 시스템: `ParticleSystemSceneProxy` 또는 mesh dynamic data adapter의 camera-facing / axis-lock transform builder.
- Mesh motion blur payload는 보존되어 있지만 Jungle 렌더 백엔드에서 아직 UE처럼 소비하지 않는다.
  - UE 원본 역할: previous transform data를 motion blur 렌더링에 사용한다.
  - Jungle에 없는 기반: particle mesh motion blur render path.
  - 나중에 연결할 시스템: mesh particle용 motion blur shader / render-state path.

## Beam2

- Actor, emitter, particle, branch beam lookup은 stub이다.
  - 현재 지원: default source/target, UserSet source/target, distance beam.
  - 현재 미지원: Actor lookup, Emitter lookup, Particle lookup, Branch beam.
  - 나중에 연결할 시스템: component instance parameter, emitter-name lookup, particle-source lookup, branch beam ownership.
- UE debug/direct-line render helper는 포팅하지 않았다.
  - UE 원본 역할: PDI를 통한 source-target direct line, tessellation visualization, debug line 렌더링.
  - Jungle에 없는 기반: 대응되는 particle debug draw / PDI path.
  - 나중에 연결할 시스템: 필요 시 engine debug draw adapter.
- Beam dynamic fill은 engine adapter다.
  - UE 원본 역할: `DoBufferFill`, `FillIndexData`, `FillVertexData_NoNoise`, `FillData_Noise`, `FillData_InterpolatedNoise`가 beam trail vertex factory용 UE dynamic buffer를 채운다.
  - Jungle 현재 동작: 같은 함수 경계를 유지하고 replay/payload를 읽기만 한 뒤 CPU-side triangle-list vertices/indices를 만든다.
  - 나중에 연결할 시스템: UE 렌더 백엔드 수준의 완전 parity가 필요하면 strip/degenerate index 생성과 view uniform 동작을 더 직접적으로 포팅한다.

## Ribbon

- Actor / particle source lookup은 stub이다.
  - UE 원본 역할: `ResolveSource` / `ResolveSourcePoint`에서 Actor source, particle-source emitter, source offset, source timing을 resolve한다.
  - Jungle에 없는 기반: Actor lookup, source name 기반 emitter lookup, particle source mapping, source offset instance parameter.
  - 나중에 연결할 시스템: component instance parameter adapter, emitter instance name lookup.
- Event generator spawn dispatch는 포팅하지 않았다.
  - UE 원본 역할: Ribbon spawn path에서 spawn/burst event generator를 호출할 수 있다.
  - Jungle에 없는 기반: UE particle event generator payload / dispatch path.
  - 나중에 연결할 시스템: event generator module support, event payload routing.
- Ribbon render fill은 engine adapter다.
  - UE 원본 역할: `FDynamicTrailsEmitterData::FillIndexData`와 `FDynamicRibbonEmitterData::FillVertexData`가 view/camera data와 dynamic parameter를 사용해 strip 기반 UE render buffer를 채운다.
  - Jungle 현재 동작: 함수 책임은 분리했고, replay/payload는 read-only로 읽으며, `ParticleSystemSceneProxy`용 CPU-side triangle-list vertices/indices를 만든다.
  - 나중에 연결할 시스템: view-dependent camera-up / render-axis uniform 동작. RHI parity가 필요하면 UE strip/degenerate backend.
- Source offset parameter가 아직 연결되어 있지 않다.
  - UE 원본 역할: 이름 기반 `TrailSourceOffset` instance parameter로 source position을 offset할 수 있다.
  - Jungle에 없는 기반: trail source offset용 particle system instance parameter parsing.
  - 나중에 연결할 시스템: vector/scalar trail source offset instance parameter adapter.

## 현재 검증 상태

- Mesh, Beam2, Ribbon Tick 호출 순서는 UE Cascade CPU 원본과 비교했다.
- Beam/Ribbon의 `GetDynamicData -> FillReplayData -> BuildMeshData` 경로는 연결되어 있다.
- 최근 점검에서 아래 금지 패턴은 발견되지 않았다.
  - `FParticleRibbonEmitterInstance::PostSpawn`
  - `FParticleBeam2EmitterInstance::Spawn(float)`
  - `FParticleMeshEmitterInstance::PreSpawn`
  - trail prev/next null을 raw `0`으로 판정하는 코드
  - Beam modifier 최종 적용을 BeamSource / BeamTarget에서 수행하는 코드
  - UE VertexFactory / RHI / FMeshBatch 포팅 시도
- 최신 Ribbon 수정 이후 Debug x64 빌드가 통과했다.
