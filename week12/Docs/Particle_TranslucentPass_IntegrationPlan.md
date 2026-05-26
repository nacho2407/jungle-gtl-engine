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
6. OIT is a future investigation, not part of this pass integration.

## Non-Goals

- Perfect translucent sorting.
- Weighted blended OIT or other OIT implementation.
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

### Step 1 - Teach Translucent Pass Instanced Drawing

Tasks:
- Add instanced draw support to `FTranslucentRenderPass::DrawEachCommand()`.
- If command instance buffer view is valid:
  - bind vertex stream 0 for mesh or quad vertices
  - bind vertex stream 1 for instance data
  - call `DrawIndexedInstanced(...)`
- Otherwise keep current non-instanced draw behavior.

Validation:
- Existing translucent static/skeletal/procedural commands still draw.
- A prepared instanced command can draw through `FTranslucentRenderPass`.

### Step 2 - Add Particle Sprite Vertex Factory or Command Type Support

Tasks:
- Decide how sprite particle quads are represented in the translucent path.
- Preserve the current sprite instance shape:
  - center
  - axis X
  - axis Y
  - color
- Add a particle sprite vertex factory or equivalent shader program selection path.
- Keep material parameter binding compatible with current sprite particle behavior.

Validation:
- Sprite particles render with the same texture/material behavior as before.
- Particle command does not require `FTranslucentRenderPass` to inspect emitter replay data.

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

Tasks:
- Remove `FParticleRenderPass` from `FRenderPipeline` members and pass list.
- Remove particle pass perf label.
- Remove or deprecate `ERenderPass::Particle`.
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

## Decisions Needed

1. Whether sprite particles should use a dedicated `EVertexFactoryType::ParticleSprite` or a more generic instanced billboard factory.
   - Suggested: dedicated `ParticleSprite` first, because its instance data is center/axis/color rather than matrix.
2. Whether `ERenderPass::Particle` is deleted immediately or kept as a deprecated empty bucket for one transition commit.
   - Suggested: delete once no command submits to it, but keep the work in a separate commit from proxy introduction.
3. Whether particle material blend state should always be material-driven in translucent pass.
   - Suggested: yes, matching current particle pass behavior.
4. Whether particle commands participate in selection mask or editor picking.
   - Suggested: no for this phase, matching `UParticleSystemComponent::SupportsOutline() == false`.
5. Whether to create an OIT follow-up document now or leave it as a future investigation.
   - Suggested: create a short follow-up only after translucent integration exposes concrete artifacts.

## Verification

- `ReleaseBuild.bat` succeeds.
- Sprite particles render through `FTranslucentRenderPass`.
- No command uses `ERenderPass::Particle`.
- Emitter pivot or bounds sorting visibly changes order as expected.
- Within-emitter particle sorting is preserved.
- Known non-guarantees are documented in code comments or this plan.
