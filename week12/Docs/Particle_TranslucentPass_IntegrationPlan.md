# Particle Translucent Pass Integration Plan

## Goal

After particle render proxy ownership is established, remove the dedicated particle render pass and draw particles through the regular translucent render path. Particle commands should become prepared instanced render commands consumed by `FTranslucentRenderPass`.

Sorting is intentionally limited: particles are sorted within an emitter, and emitter commands are sorted by emitter pivot or command bounds. Particle-level interleaving across different emitters or regular translucent primitives is not guaranteed.

## Dependency

This plan depends on `Docs/Particle_RenderProxy_IntroductionPlan.md`.

Do not start this work until:
- `UParticleSystemComponent` routes through `FParticleSystemRenderProxy`.
- Particle instance buffers are owned by the proxy/render resource, not `FParticleRenderPass`.
- Existing sprite particles still render through the old particle pass with proxy-prepared instance buffers.

## Fixed Decisions

1. Particle instancing remains the only initial instancing user.
2. Skeletal mesh instancing is not supported.
3. Mesh particles do not cast shadows in this phase.
4. Particle sorting guarantees are limited to:
   - within-emitter particle ordering
   - emitter-command ordering by pivot or bounds center
5. Sorting does not guarantee:
   - per-particle interleaving across different emitters
   - per-particle interleaving with non-particle translucent primitives
   - triangle-level sorting
6. OIT is not part of this pass integration. Do not create or implement an OIT follow-up in this scope.
7. Sprite particles use a dedicated `EVertexFactoryType::ParticleSprite` first. The instance layout is center/axis/color, not a matrix.
8. Sprite particle quad/index buffers should move behind a shared render resource access point, preferably `FResourceManager`, so later mesh particle support can reuse the same command path without making `FTranslucentRenderPass` own particle-only geometry.
9. Particle commands keep the existing explicit VFX particle shader selection path for this phase. Do not require particle materials to route through the general surface/translucent material shader type before the pass integration is stable.
10. Particle material blend state remains material-driven.
11. Particle commands do not participate in selection mask or editor picking in this phase, matching `UParticleSystemComponent::SupportsOutline() == false`.
12. Keep `FParticleRenderPass` and `ERenderPass::Particle` until sprite particles have been verified to render correctly through the translucent pass. Remove them only after that behavior is confirmed.

## Non-Goals

- Perfect translucent sorting.
- Weighted blended OIT or other OIT implementation.
- OIT follow-up planning for this phase.
- Depth pre-pass integration for particles.
- Shadow casting for mesh particles.
- Global particle batching across emitters.
- Skeletal particle instancing.

## Current Code Anchors

- Dedicated particle pass: `JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.*`
- Translucent pass: `JSEngine/Source/Engine/Render/Renderer/RenderFlow/TranslucentRenderPass.*`
- Pipeline pass list: `JSEngine/Source/Engine/Render/Renderer/RenderFlow/RenderPipeline.*`
- Render pass enum: `JSEngine/Source/Engine/Render/Common/RenderTypes.h`
- Render command contract: `JSEngine/Source/Engine/Render/Scene/RenderCommand.h`
- Particle proxy plan: `Docs/Particle_RenderProxy_IntroductionPlan.md`

## Target Shape

```cpp
FParticleSystemRenderProxy
  -> builds particle instance buffer
  -> emits FRenderCommand to ERenderPass::Translucent

FTranslucentRenderPass
  -> sorts command-level translucent work
  -> binds instance buffer when present
  -> draws sprite or mesh particle commands through the normal pass
```

## Implementation Steps

### Step 0 - Prepare Particle Sprite Render Resources and Shader Policy

Tasks:
- Add shared access for the sprite quad vertex/index buffers outside `FParticleRenderPass`, preferably through `FResourceManager`.
- Add `EVertexFactoryType::ParticleSprite` or an equivalent dedicated descriptor for the existing sprite input layout:
  - quad position/UV in stream 0
  - center/axis X/axis Y/color in stream 1
- Keep particle command drawing on the explicit `VFXParticle` shader path used by the current particle pass.
- Do not require particle materials to declare `ShaderType = VFXParticle` for this integration step.
- Keep material parameter and blend-state binding compatible with current sprite particle behavior.
- Define resource lifetime/release behavior for the shared quad resource, including lazy creation and release during renderer/resource shutdown.

Validation:
- `FParticleRenderPass` can still draw sprite particles using the shared quad resource while it remains in the pipeline.
- The shared quad resource can be used by `FTranslucentRenderPass` without depending on emitter replay data.

### Step 1 - Teach Translucent Pass Particle Instanced Drawing

Tasks:
- Add instanced draw support to `FTranslucentRenderPass::DrawEachCommand()`.
- If a command is a particle sprite command with a valid instance buffer view:
  - bind the shared sprite quad vertex/index buffers for stream 0
  - bind vertex stream 1 for instance data
  - bind the explicit `VFXParticle` shader program
  - bind material parameters and blend state using the command material
  - call `DrawIndexedInstanced(...)`
- Otherwise keep current non-instanced draw behavior.
- Keep the particle branch scoped so D3D state changes do not break later normal translucent mesh draws in the same pass.

Validation:
- Existing translucent static/skeletal/procedural commands still draw.
- A prepared instanced command can draw through `FTranslucentRenderPass`.
- Particle draw does not leak incompatible vertex buffers, shader state, blend/depth/rasterizer state, sampler state, or SRV bindings into following translucent commands.

### Step 2 - Finalize Particle Sprite Command Contract

Tasks:
- Represent sprite particle quads through `EVertexFactoryType::ParticleSprite`.
- Preserve the current sprite instance shape:
  - center
  - axis X
  - axis Y
  - color
- Keep particle-specific shader selection inside the particle command branch, not in the general material surface path.
- Set command bounds or pivot per emitter command, not only from the owning component's aggregate bounds.

Validation:
- Sprite particles render with the same texture/material behavior as before.
- Particle command does not require `FTranslucentRenderPass` to inspect emitter replay data.
- Materials with general `ShaderType = None` but valid particle parameters still render through the forced `VFXParticle` command path.

### Step 3 - Route Particle Proxy Commands to Translucent

Tasks:
- Change `FParticleSystemRenderProxy` to submit sprite particle commands to `ERenderPass::Translucent`.
- Set command bounds/pivot data so `TranslucentRenderPass` can sort emitter commands.
- Keep within-emitter particle sorting inside the proxy before instance buffer upload.

Validation:
- Sprite particles render through `FTranslucentRenderPass`.
- Existing `ERenderPass::Particle` queue is no longer required for sprite particles.

### Step 4 - Extend Translucent Sort Key Policy

Tasks:
- Keep current translucent command sorting for normal mesh commands.
- For particle commands, sort by:
  - command `WorldAABB.GetCenter()` if valid
  - otherwise emitter/component pivot
- Document that this is emitter-command sorting, not global particle sorting.

Validation:
- Particle emitter order changes predictably as the camera moves around emitters.
- Within a single emitter, particle order remains controlled by the proxy's active particle sorting.

### Step 5 - Remove Particle Render Pass from Pipeline

Do not start this step until sprite particles have been verified to render correctly through `FTranslucentRenderPass`.

Tasks:
- Remove `FParticleRenderPass` from `FRenderPipeline` members and pass list.
- Remove particle pass perf label.
- Remove `ERenderPass::Particle`.
- Delete or quarantine `ParticleRenderPass.*` after no users remain.
- Update project files if source files are removed.

Validation:
- Pipeline still forwards `PrevPassSRV` and `PrevPassRTV` correctly.
- Fog, post-process, FXAA, font, and selection outline still run.
- No code path submits to `ERenderPass::Particle`.

## Concerns

1. `FTranslucentRenderPass` currently assumes material surface mesh commands. Sprite particle commands may need a clean vertex factory/program branch to avoid particle-specific logic leaking everywhere.
2. Removing `ParticleRenderPass` changes pass order. Current order should be checked carefully so particles still render before post-process and after depth-producing passes.
3. Command-level sorting is not equivalent to full particle-level sorting. Visual artifacts between overlapping emitters are expected.
4. If mesh particles later route to `Opaque`, they must not automatically enter shadow/depth-pre policies unless explicitly allowed.
5. Editor picking and selection outline currently include selected surface commands by pass. Particle components currently do not support outline; keep that policy explicit.
6. If `ERenderPass::Particle` is removed, all stale references must be cleaned from project files, perf labels, and any debug tooling.
7. The current sprite particle material path works even when a material's general `ShaderType` is not `VFXParticle`. The translucent integration should preserve that behavior until particle material asset policy is explicitly revised.
8. The current proxy uses component-level bounds for particle commands. Translucent command sorting needs emitter-level bounds or pivot data to avoid identical sort keys for multiple emitters on one component.
9. Shared particle quad buffers need explicit lifetime rules. Avoid hiding device-context work in `FResourceManager`; prefer device-only lazy creation and a clear release path.
10. The particle branch in `FTranslucentRenderPass` must restore or overwrite all state needed by subsequent normal translucent commands.
11. Particle material parameter binding must be validated against the forced `VFXParticle` shader path, especially `DiffuseMap` and `Opacity`.
12. Emitter-level bounds for sprite particles must respect the snapshot coordinate-space policy and particle size. Do not reapply component transforms to snapshots that are already in world space.
13. Mesh particle support remains a future extension. This phase should not mix sprite integration with mesh section routing, opaque/translucent material splitting, or mesh instance transform layout work.

## Decisions Needed

None for this phase. The remaining open work is implementation and verification.

## Verification

- `ReleaseBuild.bat` succeeds.
- Sprite particles render through `FTranslucentRenderPass`.
- Sprite particles still render after `FParticleRenderPass` is removed.
- No command uses `ERenderPass::Particle`.
- Emitter pivot or bounds sorting visibly changes order as expected.
- Within-emitter particle sorting is preserved.
- Known non-guarantees are documented in code comments or this plan.
