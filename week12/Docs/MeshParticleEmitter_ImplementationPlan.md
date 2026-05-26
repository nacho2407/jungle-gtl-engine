# Mesh Particle Emitter Implementation Plan

## Goal

현재 billboard sprite 중심의 particle renderer를 Cascade 스타일의 type data 확장 구조에 맞춰 mesh particle까지 지원한다. 첫 구현은 `mesh particle 1개 = static mesh instance 1개`를 목표로 하며, trail/beam/ribbon 확장은 이번 문서의 후속 정책 항목으로만 남긴다.

## Fixed Policies

1. Mesh particle은 billboard 정렬을 수행하지 않는다.
   - snapshot의 particle 위치는 local 좌표계로 고정한다.
   - 렌더링 과정에서 particle local transform과 emitter `ComponentToWorld`를 합성해 world space에 배치한 뒤 view/projection 변환한다.
2. 기존 `ParticleRenderPass`는 `TranslucentParticleRenderPass`로 이름을 변경한다.
   - 새 `OpaqueParticleRenderPass`를 `TranslucentParticleRenderPass` 바로 앞에 추가한다.
   - `OpaqueParticleRenderPass`는 depth test와 depth write를 사용한다.
3. Mesh particle은 `DepthPrePass`, `ShadowPass`, light/decal 수집 대상에 편입하지 않는다.
   - 즉, opaque mesh particle도 그림자를 드리우지 않는다.
   - 이 pass는 일반 opaque pass 완전 편입이 아니라 translucent 직전의 late opaque particle pass다.
4. Material은 static mesh section material을 그대로 사용한다.
   - `UParticleModuleRequired::Material`은 mesh emitter 렌더링에서 사용하지 않는다.
   - particle color multiply, per-particle material override는 지원하지 않는다.
5. Multi-material static mesh는 기존 mesh rendering처럼 section별로 draw한다.
6. Sorting은 particle origin 기준으로만 수행한다.
   - Triangle-level sorting은 수행하지 않는다.
7. Rotation은 현재 `FBaseParticle::Rotation` 기반 로직에서 가능한 수준만 사용한다.
   - 정확한 3D 회전은 후속 mesh 전용 rotation module에서 처리한다.
8. Bounds 정확도는 첫 구현에서 보장하지 않는다.
   - 단, culling/viewer framing/raycast에 영향을 줄 수 있음을 TODO로 명시하고 후속 phase에서 정책 결정 또는 구현한다.

## Current Code Anchors

- Particle snapshot/type data: `JSEngine/Source/Engine/Particle/ParticleTypes.h`, `ParticleModules.*`, `ParticleEmitterInstance.*`
- Particle component snapshot collection: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`
- Render command collection: `JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp`
- Render pass enum: `JSEngine/Source/Engine/Render/Common/RenderTypes.h`
- Current particle pass: `JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.*`
- Pipeline order: `JSEngine/Source/Engine/Render/Renderer/RenderFlow/RenderPipeline.*`
- Existing opaque/translucent draw behavior: `OpaqueRenderPass.*`, `TranslucentRenderPass.*`

## Phase 0 - Contract Cleanup

### Step 0.1 - Name the render passes

Tasks:
- Rename `FParticleRenderPass` to `FTranslucentParticleRenderPass`.
- Rename `ParticleRenderPass.*` files or introduce wrapper files only if project-file churn is too large.
- Add `FOpaqueParticleRenderPass`.
- Update pipeline member names, includes, perf labels, and release paths.

Validation:
- Build succeeds with no behavior change for existing sprite particles.
- Current sprite particles still render through `TranslucentParticleRenderPass`.

### Step 0.2 - Split particle render pass enum values

Tasks:
- Replace the single `ERenderPass::Particle` bucket with explicit particle buckets, for example:
  - `OpaqueParticle`
  - `TranslucentParticle`
- Keep pass names aligned with `FRenderPipeline::RenderPasses` order.
- Make command collection reject unsupported particle types with a clear diagnostic path instead of silently treating them as sprite.

Validation:
- Existing sprite emitters route to `TranslucentParticle`.
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
- Add diagnostics for missing mesh, missing section material, and empty active particles as separate cases.

Validation:
- Setting `RequiredModule.Material` on a mesh emitter does not affect rendering.
- Missing mesh fails with a useful log/stat, not a crash.

## Phase 2 - Command Routing

### Step 2.1 - Accept mesh emitter snapshots

Tasks:
- Remove the sprite-only filter in particle command collection.
- Route sprite data to `TranslucentParticle`.
- Route mesh data by static mesh section material:
  - opaque section material -> `OpaqueParticle`
  - translucent section material -> `TranslucentParticle`
- Keep `DepthPrePass`, `ShadowPass`, light/decal collection unchanged.

Validation:
- Sprite emitters still render.
- Mesh emitter with opaque material reaches only `OpaqueParticle`.
- Mesh emitter with translucent material reaches only `TranslucentParticle`.
- A mesh with mixed section materials emits commands for both particle passes.

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
  - optional reserved custom data slots for future extension, left unused in MVP
- Do not multiply particle color into material output.

Validation:
- Particle `Location` and `Size` affect mesh placement.
- Billboard camera axes are not used for mesh particles.

### Step 3.2 - Add mesh particle shader path

Tasks:
- Prefer a dedicated instanced mesh particle vertex shader path if current static mesh vertex factories cannot accept an instance transform cleanly.
- Reuse the static mesh material pixel shader path and material parameter binding.
- Keep material blend/depth state behavior consistent with the section material.

Validation:
- Opaque mesh particle draws with depth write.
- Translucent mesh particle draws with depth read and material blend state.
- Texture/material parameters from the static mesh material are visible.

### Step 3.3 - Draw section instances

Tasks:
- Bind static mesh vertex/index buffers.
- Bind the mesh particle instance buffer as a second vertex stream, or use the closest existing engine pattern if one already exists.
- Draw each section with `DrawIndexedInstanced`.
- Preserve section index start/count semantics from static mesh rendering.

Validation:
- One particle draws one full static mesh.
- N particles draw N instances without CPU-side vertex duplication.
- Mixed material sections retain their material assignment.

## Phase 4 - Sorting and Pass Interaction

### Step 4.1 - Particle-origin sorting

Tasks:
- Reuse the current particle active-index sort helper for mesh particles.
- Sort by particle origin only.
- Apply sorting only where pass semantics need it:
  - `TranslucentParticle`: back-to-front when requested
  - `OpaqueParticle`: keep stable/no sort unless a clear need appears

Validation:
- Translucent mesh particles sort against each other by origin.
- Triangle-level sorting remains unsupported and documented.

### Step 4.2 - Clarify translucent interop limits

Tasks:
- Decide whether `TranslucentParticleRenderPass` runs before or after existing `TranslucentRenderPass`.
- Document that regular translucent primitives and translucent particles are not globally sorted together in the MVP.
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

### Step 7.1 - Bounds policy

Tasks:
- Decide whether mesh particle bounds are:
  - conservative emitter-level bounds,
  - per-particle transformed mesh bounds,
  - or fixed user-authored bounds from `RequiredModule`.
- Implement the selected policy for culling, viewer framing, and raycast behavior.
- Remove the MVP TODO once implemented.

Validation:
- Mesh particles are not culled incorrectly when the component or particles move.
- Viewer framing includes mesh extents, not only particle origins.

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
- If enabled, define whether mesh particles become regular primitive commands or stay in particle-owned pass buckets.

Validation:
- Shadow/depth behavior is intentional and documented.
- Existing late opaque particle behavior remains available or migrates cleanly.

### Step 7.4 - Batching and performance

Tasks:
- Batch across emitters only after correctness is stable.
- Candidate batch key:
  - mesh resource
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
- Opaque mesh particles draw in `OpaqueParticleRenderPass` with depth write, immediately before `TranslucentParticleRenderPass`.
- Mesh particle material comes from static mesh sections only.
- No color multiply, no shadow casting, no DepthPre integration, no accurate mesh bounds, no triangle-level sorting.
