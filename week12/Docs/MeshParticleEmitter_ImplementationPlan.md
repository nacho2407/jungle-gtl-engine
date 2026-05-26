# Mesh Particle Emitter Implementation Plan

## Goal

현재 billboard sprite 중심의 particle renderer를 Cascade 스타일의 type data 확장 구조에 맞춰 mesh particle까지 지원한다. 첫 구현은 `mesh particle 1개 = static mesh instance 1개`를 목표로 하며, trail/beam/ribbon 확장은 이번 문서의 후속 정책 항목으로만 남긴다.

## Fixed Policies

1. Mesh particle은 billboard 정렬을 수행하지 않는다.
   - snapshot의 particle 위치는 local 좌표계로 고정한다.
   - 렌더링 과정에서 particle local transform과 emitter `ComponentToWorld`를 합성해 world space에 배치한 뒤 view/projection 변환한다.
2. 현재 소스 기준으로 별도 `ParticleRenderPass`는 재도입하지 않는다.
   - Sprite particle은 이미 `FParticleSystemRenderProxy`가 준비한 instance command를 `ERenderPass::Translucent`로 제출한다.
   - Mesh particle도 proxy가 command를 준비하고 기존 surface pass인 `ERenderPass::Opaque` / `ERenderPass::Translucent`로 제출한다.
   - Opaque mesh particle은 기존 `FOpaqueRenderPass`의 depth test/depth write 정책을 사용한다.
3. Mesh particle은 `DepthPrePass`, `ShadowPass`, light/decal 수집 대상에 편입하지 않는다.
   - 즉, opaque mesh particle도 그림자를 드리우지 않는다.
   - 이 단계의 opaque mesh particle은 regular opaque command로 그리되 shadow/depth-pre 수집에는 들어가지 않는다.
4. Material은 static mesh section material을 그대로 사용한다.
   - `UParticleModuleRequired::Material`은 mesh emitter 렌더링에서 사용하지 않는다.
   - particle color multiply, per-particle material override는 지원하지 않는다.
   - Pass routing은 기존 surface policy인 `ResolveMaterialRenderPass()`를 따른다. 즉, 현 정책에서는 `ShaderType`이 pass routing source이고, `BlendMode` / `BlendState`는 render-state data다.
5. Multi-material static mesh는 기존 mesh rendering처럼 section별로 draw한다.
6. Sorting은 particle origin 기준으로만 수행한다.
   - Triangle-level sorting은 수행하지 않는다.
7. Rotation은 현재 `FBaseParticle::Rotation` 기반 로직에서 가능한 수준만 사용한다.
   - 정확한 3D 회전은 후속 mesh 전용 rotation module에서 처리한다.
8. Bounds는 MVP에서 conservative mesh particle bounds까지만 포함한다.
   - `RequiredModule.bUseFixedBounds == true`면 기존 fixed bounds를 우선 적용한다.
   - fixed bounds가 없으면 particle origin, `FBaseParticle::Size`, static mesh `LocalBounds`를 합성한 보수적 AABB를 사용한다.
   - 원본 mesh 단위/스케일 보정, update module로 변하는 size/scale, mesh 전용 3D rotation, 정확한 per-particle bounds, raycast/picking 정확도는 후속으로 남긴다.
9. Mesh particle MVP는 static mesh LOD 0만 사용한다.
   - 카메라 거리 기반 LOD 선택과 emitter별 LOD resource 캐시는 후속 batching/performance 단계에서 다룬다.
10. Mesh particle MVP는 debug view mode, selection mask, editor picking을 지원하지 않는다.
   - 일반 render pass에서 보이는 것과 particle viewer framing까지만 acceptance에 포함한다.
11. Mesh particle MVP의 lighting 정확도는 uniform scale 기준으로만 보장한다.
   - non-uniform `FBaseParticle::Size`는 transform/bounds에는 반영하되 normal transform 정확도는 후속 instance normal-matrix payload에서 해결한다.

## Resource Ownership Policy

- `FParticleSystemRenderProxy`는 mesh particle instance buffer처럼 proxy가 직접 생성하는 dynamic resource만 소유한다.
- `FMeshBuffer`의 소유자는 계속 `FMeshBufferManager`다.
- Proxy는 `FMeshBuffer*`를 멤버로 장기 캐시하지 않는다. Reimport, LOD, renderer resource rebuild 시 stale pointer가 될 수 있기 때문이다.
- Proxy는 `CollectCommands()` 중 `FPrimitiveRenderProxyCollectionContext`가 제공하는 resource resolver를 통해 `UStaticMesh* + LOD 0 -> FMeshBuffer*`를 해석하고, 그 프레임의 `FRenderCommand`에 non-owning pointer로만 넣는다.
- 현재 `FScene` / render resource abstraction이 없으므로 `FMeshBufferManager&`를 context에 직접 추가해도 된다. 단, 이는 과도기적 concrete dependency이며 장기적으로는 `FRenderResourceProvider` / `FSceneRenderResourceContext` 같은 얇은 인터페이스로 대체한다.

## Current Code Anchors

- Particle snapshot/type data: `JSEngine/Source/Engine/Particle/ParticleTypes.h`, `ParticleModules.*`, `ParticleEmitterInstance.*`
- Particle component snapshot collection: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`
- Particle render proxy command collection: `JSEngine/Source/Engine/Render/Scene/ParticleSystemRenderProxy.*`
- Proxy collection context: `JSEngine/Source/Engine/Render/Scene/PrimitiveRenderProxy.h`
- Legacy primitive command collection reference: `JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp`
- Render pass enum: `JSEngine/Source/Engine/Render/Common/RenderTypes.h`
- Pipeline order: `JSEngine/Source/Engine/Render/Renderer/RenderFlow/RenderPipeline.*`
- Existing opaque/translucent draw behavior: `OpaqueRenderPass.*`, `TranslucentRenderPass.*`
- Mesh GPU buffer owner: `JSEngine/Source/Engine/Render/Resource/MeshBufferManager.*`

## Phase 0 - Contract Cleanup

### Step 0.1 - Align with current proxy/pass topology

Tasks:
- Remove stale implementation assumptions that mention `FParticleRenderPass`, `OpaqueParticleRenderPass`, `TranslucentParticleRenderPass`, or `ERenderPass::Particle`.
- Treat `FParticleSystemRenderProxy` as the particle command preparation owner.
- Treat `FOpaqueRenderPass` and `FTranslucentRenderPass` as the MVP draw passes for mesh particle commands.
- Keep sprite particle behavior unchanged: sprite commands continue to be prepared by proxy and drawn through `FTranslucentRenderPass`.

Validation:
- Build succeeds with no behavior change for existing sprite particles.
- Current sprite particles still render through `FTranslucentRenderPass`.

### Step 0.2 - Add a mesh buffer resolver to the proxy context

Tasks:
- Add `FMeshBufferManager&` to `FPrimitiveRenderProxyCollectionContext` as the current concrete resource resolver.
- Pass `RenderCollector`'s existing `MeshBufferManager` into `RenderProxy->CollectCommands(...)`.
- Use the resolver only inside `FParticleSystemRenderProxy::CollectCommands()` / mesh command build helpers.
- Do not store `FMeshBuffer*` as a long-lived proxy member.
- Store the resolved `FMeshBuffer*` only in the generated `FRenderCommand` for the current frame.
- Leave a TODO or comment noting that `FMeshBufferManager&` should become an abstract render resource provider when `FScene` or an equivalent render scene layer exists.

Validation:
- Existing sprite emitters still route to `ERenderPass::Translucent`.
- Mesh command building can resolve static mesh LOD 0 buffer without re-querying particle TypeData from the renderer side.
- Empty particle systems and disabled show flags still no-op cleanly.

## Phase 1 - Mesh Snapshot Contract

### Step 1.1 - Local-space mesh snapshot

Tasks:
- Update `UParticleModuleTypeDataMesh::GetDynamicRenderData()` so copied mesh particle locations remain in emitter local space.
- Set mesh replay data to:
  - `CoordinateSpace = Local`
  - `ComponentToWorld = InEmitterInstance->GetOwner().GetComponentToWorld()`
  - `Scale = OneVector`
- Renderer must apply `ComponentToWorld` when building each mesh particle instance transform.
- Preserve `ParticleIndices` indirection and `ParticleStride`; renderer must continue to use `GetParticleByActiveIndex()`.

Validation:
- Moving the owning component moves mesh particles through renderer-side local-to-world conversion.
- Mesh particle snapshot data is not pre-transformed to world space.
- Renderer does not double-transform mesh particles.

### Step 1.2 - Make mesh material ownership explicit

Tasks:
- Document in code comments that mesh emitter ignores `UParticleModuleRequired::Material`.
- Ensure `FDynamicMeshEmitterData::Mesh` is the renderer-facing source for section materials.
- Resolve missing section materials the same way the existing static mesh path does: fall back to `DefaultWhite`.
- Add low-noise diagnostics for missing mesh, default material fallback, and empty active particles as separate cases.

Validation:
- Setting `RequiredModule.Material` on a mesh emitter does not affect rendering.
- Missing mesh fails with a useful log/stat, not a crash.
- Missing section material renders with `DefaultWhite` and does not skip the whole mesh particle command.

## Phase 2 - Command Routing

### Step 2.1 - Accept mesh emitter snapshots

Tasks:
- Remove the sprite-only filter in particle command collection.
- Route sprite data to `ERenderPass::Translucent`.
- Route mesh data by static mesh section material using the existing surface routing policy:
  - section material resolved by `ResolveMaterialRenderPass()` to `ERenderPass::Opaque`
  - section material resolved by `ResolveMaterialRenderPass()` to `ERenderPass::Translucent`
- Keep `DepthPrePass`, `ShadowPass`, light/decal collection unchanged.
- Do not add `EPrimitiveType::EPT_ParticleSystem` to shadow/depth-pre renderable primitive filters.
- Do not route mesh particles to `ViewModeMesh`, `SelectionMask`, or editor picking paths in the MVP.

Validation:
- Sprite emitters still render.
- Mesh emitter with opaque-routed material reaches only `ERenderPass::Opaque`.
- Mesh emitter with translucent-routed material reaches only `ERenderPass::Translucent`.
- A mesh with mixed section materials emits commands for both surface passes.
- Mesh particles remain absent from debug view mode, selection mask, and picking flows by policy.

### Step 2.2 - Decide command granularity

Tasks:
- Use `Mesh + Section + Material + ParticleEmitterSnapshot` as the first command granularity.
- Do not batch across emitters in the first implementation.
- Store enough section metadata in `FRenderCommand` or a particle-specific command payload to draw the same section range used by static mesh rendering.

Validation:
- Multi-material mesh draws all valid sections once per active particle.
- Invalid or empty sections are skipped without affecting other sections.

## Phase 3 - Instanced Mesh Particle Rendering

### Step 3.1 - Build per-particle instance data

Tasks:
- Convert each active `FBaseParticle` into per-instance mesh data.
- Instance data should include at minimum:
  - world transform matrix
  - enough data for the vertex shader to transform normals correctly under uniform scale
  - optional reserved custom data slots for future extension, left unused in MVP
- Treat uniform scale as the supported lighting path for the MVP. Non-uniform `FBaseParticle::Size` may affect placement/bounds, but exact normal correction is deferred.
- Do not multiply particle color into material output.

Validation:
- Particle `Location` and `Size` affect mesh placement.
- Billboard camera axes are not used for mesh particles.
- Uniform-scale mesh particles have correct-looking lighting.
- Non-uniform size limitations are documented and do not block MVP acceptance.

### Step 3.2 - Add mesh particle shader path

Tasks:
- Add a dedicated `EVertexFactoryType::ParticleMesh` and instanced mesh particle vertex shader path unless an equivalent existing instance-capable path exists by implementation time.
- Reuse the static mesh material pixel shader path and material parameter binding.
- Keep material blend/depth state behavior consistent with the section material.
- Add scoped mesh particle branches to both `FOpaqueRenderPass` and `FTranslucentRenderPass`, for example `IsParticleMeshCommand()` / `DrawParticleMeshCommand()`.
- The pass branch must bind static mesh vertex/index buffers plus the mesh particle instance buffer as stream 1 and call `DrawIndexedInstanced`.

Validation:
- Opaque mesh particle draws with depth write.
- Translucent mesh particle draws with depth read and material blend state.
- Texture/material parameters from the static mesh material are visible.
- Normal static mesh and translucent mesh commands still use their existing non-instanced draw path.

### Step 3.3 - Draw section instances

Tasks:
- Bind static mesh vertex/index buffers.
- Bind the mesh particle instance buffer as a second vertex stream, or use the closest existing engine pattern if one already exists.
- Draw each section with `DrawIndexedInstanced`.
- Preserve section index start/count semantics from static mesh rendering.
- Use static mesh LOD 0 for all MVP mesh particle commands.

Validation:
- One particle draws one full static mesh.
- N particles draw N instances without CPU-side vertex duplication.
- Mixed material sections retain their material assignment.
- Static mesh LOD switching is explicitly not expected in this phase.

### Step 3.4 - Add MVP Conservative Mesh Particle Bounds

Tasks:
- Keep `RequiredModule.bUseFixedBounds` as the highest-priority bounds policy.
- When fixed bounds are disabled, compute mesh emitter bounds from active particle origins plus static mesh `LocalBounds` transformed by the same MVP instance transform used for drawing.
- Use `FBaseParticle::Size` as the MVP scale input. Do not attempt to correct arbitrary source mesh unit scale or future mesh-specific scale modules in this phase.
- Ignore exact 3D mesh rotation for bounds except for whatever rotation is already represented in the MVP instance transform.
- Implement one shared helper for conservative mesh particle bounds and use it for both particle component/world bounds and mesh particle command `WorldAABB`.
- Document in code comments that this is a visibility/framing/sort-key safety bound, not an exact picking/raycast contract.

Validation:
- A mesh particle whose mesh is larger than a unit sprite does not disappear due to sprite-sized bounds.
- Particle viewer framing includes visible mesh extents well enough for MVP inspection.
- Translucent mesh particle commands have a usable command bounds center for sort-key calculation.
- Component bounds and command bounds agree because they use the same helper.
- Fixed bounds still override auto conservative bounds when enabled.
- Raycast/picking accuracy remains explicitly unsupported in this phase.

## Phase 4 - Sorting and Pass Interaction

### Step 4.1 - Particle-origin sorting

Tasks:
- Reuse the current particle active-index sort helper for mesh particles.
- Sort by particle origin only.
- Apply sorting only where pass semantics need it:
  - `ERenderPass::Translucent`: back-to-front when requested
  - `ERenderPass::Opaque`: keep stable/no sort unless a clear need appears

Validation:
- Translucent mesh particles sort against each other by origin.
- Triangle-level sorting remains unsupported and documented.

### Step 4.2 - Clarify translucent interop limits

Tasks:
- Mesh/sprite particle translucent commands run through existing `FTranslucentRenderPass`.
- Document that translucent command sorting is command-level sorting, not full per-particle/per-triangle interleaving.
- If mesh particle command granularity is `Emitter + Mesh + Section + Material`, overlapping particles from different emitters or sections may still show artifacts.
- If artifacts are severe, add a future phase for unified translucent command sorting.

Validation:
- Pass order is intentional and visible in `FRenderPipeline::RenderPasses`.
- Known limitation is documented near the pass or in this plan.

## Phase 5 - Editor and Asset Flow

### Step 5.1 - Mesh asset restore

Tasks:
- Ensure `UParticleModuleTypeDataMesh::MeshAssetPath` restores `Mesh` after `.particle` load.
- Keep `SetStaticMesh()` as the single place that synchronizes raw mesh pointer and soft path.
- Avoid resolving the mesh from the renderer side.

Validation:
- Save and reload a particle system with mesh type data.
- Mesh preview still uses the selected mesh after reload.

### Step 5.2 - Editor clarity

Tasks:
- Make it clear in the particle editor that `RequiredModule.Material` is ignored by mesh emitters.
- Keep mesh asset selection on `Mesh Type Data`.
- Keep undo/redo behavior for mesh asset changes.

Validation:
- User can create or convert to mesh type data and assign a static mesh.
- Changing required material does not appear to promise mesh override behavior.

## Phase 6 - Diagnostics and Validation Assets

### Step 6.1 - Runtime diagnostics

Tasks:
- Add counters or logs for:
  - no active particles
  - missing mesh
  - missing mesh buffer
  - missing section material
  - unsupported emitter type
  - draw failure
- Keep diagnostics low-noise in normal frames.

Validation:
- Each failure mode can be triggered intentionally in the editor/viewer.
- Failures do not crash the renderer.

### Step 6.2 - Test particle assets

Tasks:
- Add or update sample `.particle` assets for:
  - sprite emitter regression
  - opaque mesh emitter
  - translucent mesh emitter
  - mixed-material mesh emitter if a suitable mesh exists
- Keep assets small and editor-friendly.

Validation:
- `ReleaseBuild.bat` succeeds.
- Particle viewer can preview all sample assets.
- Visual QA confirms depth write for opaque mesh particles and blend behavior for translucent mesh particles.

## Phase 7 - Deferred Policy and Implementation Items

### Step 7.1 - Accurate bounds, raycast, and picking policy

Tasks:
- Upgrade the MVP conservative bounds into an exact bounds contract if the project needs it.
- Define how source mesh unit scale, update-time size/scale modules, and mesh-specific 3D rotation affect bounds.
- Decide whether raycast/picking should use:
  - component-level conservative bounds only,
  - per-particle transformed mesh bounds,
  - or exact mesh triangle tests.
- Keep fixed user-authored bounds behavior compatible with the MVP policy.

Validation:
- Bounds, viewer framing, command sort bounds, and raycast/picking use an intentional shared contract.
- Mesh particles are not culled incorrectly when component transform, particle size, mesh scale, or mesh rotation changes.

### Step 7.2 - 3D mesh rotation module

Tasks:
- Add a mesh-appropriate rotation module or payload.
- Define rotation order and coordinate-space behavior.
- Keep existing `FBaseParticle::Rotation` behavior backward compatible for sprites.

Validation:
- Mesh particles can rotate independently in 3D.
- Sprite rotation behavior is unchanged.

### Step 7.3 - DepthPre, shadow, and lighting participation

Tasks:
- Revisit whether opaque mesh particles should participate in:
  - `DepthPrePass`
  - `ShadowPass`
  - light/decal culling inputs
- If enabled, define whether mesh particles remain proxy-prepared surface commands or move behind a dedicated particle render path again.

Validation:
- Shadow/depth behavior is intentional and documented.
- Existing surface-pass mesh particle behavior remains available or migrates cleanly.

### Step 7.4 - Batching and performance

Tasks:
- Batch across emitters only after correctness is stable.
- Add camera-distance static mesh LOD selection only after the LOD 0 MVP path is stable.
- Candidate batch key:
  - mesh resource
  - LOD
  - section index
  - material
  - pass
  - vertex factory/shader permutation
- Add memory accounting for particle instance buffers if existing renderer stats support it.

Validation:
- Draw call count improves for repeated mesh emitters.
- Sorting and material routing remain correct.

### Step 7.5 - Unified translucent sorting

Tasks:
- Decide whether regular translucent commands and translucent particle commands should share one sorted command list.
- If yes, define a common sort key and command abstraction that does not force sprite, mesh particle, and normal mesh paths into unsafe casts.

Validation:
- Translucent mesh, sprite particles, and regular translucent primitives sort predictably under camera movement.

## First Implementation Cut

The first coding pass should stop after Phase 3 unless blocking issues require Phase 4. The acceptance target is:

- Existing sprite particles still render.
- Mesh type data with a static mesh can render opaque and translucent section materials.
- Opaque mesh particles draw through `FOpaqueRenderPass` with depth write.
- Translucent mesh particles draw through `FTranslucentRenderPass` with material blend/depth policy.
- Mesh particle material comes from static mesh sections only.
- Missing section material falls back to `DefaultWhite`.
- Mesh particle rendering uses static mesh LOD 0 only.
- Uniform-scale mesh particles have correct-looking lighting; non-uniform normal accuracy is deferred.
- Conservative mesh particle bounds are available for visibility, viewer framing, and command sort keys.
- No color multiply, no shadow casting, no DepthPre integration, no debug view/selection/picking support, no exact bounds/raycast/picking contract, no triangle-level sorting.
