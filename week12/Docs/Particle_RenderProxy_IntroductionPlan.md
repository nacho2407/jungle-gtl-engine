# Particle Render Proxy Introduction Plan

## Goal

Particle rendering should move toward a primitive render proxy model without rewriting every primitive type at once. This first step adds the proxy seam to `UPrimitiveComponent`, routes only `UParticleSystemComponent` through the new path, and moves sprite particle instance-buffer ownership out of `FParticleRenderPass`.

The visible behavior should remain unchanged after this step: existing sprite particles still render through the current particle pass, but the pass no longer owns particle instance buffers.

## Fixed Decisions

1. Instancing is used only by particles for now.
2. Skeletal mesh instancing is not supported in this phase.
3. The proxy seam is introduced at `UPrimitiveComponent`, but only particle components opt into it.
4. Existing static mesh, skeletal mesh, procedural mesh, decal, billboard, SubUV, and editor overlay paths remain on the current collectors/builders.
5. `UParticleSystemComponent` is the lifetime anchor for the particle render proxy, but GPU resource management belongs to the render proxy/resource type.
6. The existing particle simulation and `FDynamicEmitterDataBase` snapshot creation path remains intact.

## Non-Goals

- Removing `FParticleRenderPass`.
- Routing particles through `FTranslucentRenderPass`.
- Introducing a render thread.
- Global translucent particle sorting.
- Supporting mesh particle shadows.
- Supporting skeletal mesh instancing.

## Current Code Anchors

- Primitive base seam: `JSEngine/Source/Engine/Component/PrimitiveComponent.h`
- Particle component snapshot owner: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.*`
- Current particle command collection: `JSEngine/Source/Engine/Render/Scene/PrimitiveDrawCommandBuilder.cpp`
- Render collection entry point: `JSEngine/Source/Engine/Render/Scene/RenderCollector.cpp`
- Current sprite instance buffer owner: `JSEngine/Source/Engine/Render/Renderer/RenderFlow/ParticleRenderPass.*`
- Render command contract: `JSEngine/Source/Engine/Render/Scene/RenderCommand.h`

## Target Shape

```cpp
UPrimitiveComponent
  -> virtual FPrimitiveRenderProxy* GetRenderProxy()

UParticleSystemComponent
  -> owns FParticleSystemRenderProxy lifetime
  -> keeps simulation state and frame emitter replay snapshots

FParticleSystemRenderProxy
  -> reads particle replay snapshots
  -> owns particle instance GPU buffers
  -> builds particle render commands

FParticleRenderPass
  -> binds prepared command buffers
  -> draws only
```

## Implementation Steps

### Step 1 - Add Primitive Render Proxy Seam

Tasks:
- Add a lightweight `FPrimitiveRenderProxy` interface.
- Add `virtual FPrimitiveRenderProxy* GetRenderProxy()` to `UPrimitiveComponent`, returning `nullptr` by default.
- Define a small proxy collection context instead of exposing the whole renderer. It should include only the data needed to build commands:
  - `FShowFlags`
  - `EViewMode`
  - `FRenderBus&`
  - `FMeshBufferManager&`
  - `ID3D11Device*`
  - `ID3D11DeviceContext*`

Validation:
- All existing non-particle primitive paths still compile and use the fallback collector path.

### Step 2 - Add Particle Render Proxy

Tasks:
- Add `FParticleSystemRenderProxy`.
- Add a `std::unique_ptr<FParticleSystemRenderProxy>` member to `UParticleSystemComponent`.
- Create the proxy in the component constructor or initialization path.
- Release proxy GPU resources from the component destructor, template reset, and particle reset paths.

Validation:
- Particle component creation/destruction and template changes do not leak or access released GPU resources.

### Step 3 - Route Particle Collection Through Proxy

Tasks:
- Update `FRenderCollector::CollectFromComponent()` to check `Primitive->GetRenderProxy()` first.
- If a proxy exists, call `Proxy->CollectCommands(...)` and return.
- Keep decal and all non-particle fallback behavior unchanged.
- Remove or bypass the particle case in `FPrimitiveDrawCommandBuilder` only after the proxy path is active.

Validation:
- Only `UParticleSystemComponent` uses the proxy path.
- Existing static/skeletal/procedural rendering is unchanged.

### Step 4 - Move Sprite Instance Buffer Ownership

Tasks:
- Move these responsibilities from `FParticleRenderPass` into `FParticleSystemRenderProxy` or a proxy-owned render resource:
  - CPU staging instance array
  - dynamic instance vertex buffer
  - max instance capacity
  - `EnsureInstanceBuffer(...)`
  - `Map(D3D11_MAP_WRITE_DISCARD)` upload
- Add an `FInstanceBufferView` or equivalent to `FRenderCommand`.
- Particle proxy builds the sorted sprite instance data and fills the command's instance buffer view.

Validation:
- `FParticleRenderPass` no longer owns `InstanceBuffer`, `Instances`, or `MaxInstanceCount`.
- Existing sprite particles still render.

### Step 5 - Simplify Particle Render Pass

Tasks:
- Keep `FParticleRenderPass` active for this phase.
- Let it bind:
  - quad vertex buffer
  - quad index buffer
  - command-provided instance buffer
- Let it call `DrawIndexedInstanced(...)` using command-provided instance count.
- Do not let it read `FDynamicEmitterReplayDataBase` directly.

Validation:
- Particle pass becomes a prepared-buffer draw pass.
- Sprite particle visual behavior remains unchanged.

## Concerns

1. The proxy collection context can become too broad if it exposes the whole renderer. Keep it intentionally narrow.
2. `UParticleSystemComponent::PackRenderData()` currently rebuilds `FDynamicEmitterDataBase` snapshots each frame. The proxy must not hold stale snapshot pointers beyond the frame.
3. If proxy-owned GPU buffers are released during template reset while a frame is being collected, lifetime ordering must remain clear. With no render thread, this is manageable but still needs disciplined call order.
4. Editor preview, game runtime, and object viewer may use different render pipeline entry points. The proxy path must be active in all render collection paths that draw particles.
5. Moving only particles to proxy creates a temporary hybrid path. This is acceptable for phase 1 but should be documented as transitional.

## Decisions Needed

Status: all decisions below are accepted as the Suggested option for this phase.

1. Name and location of the proxy interface files.
   - Suggested: `Render/Scene/PrimitiveRenderProxy.h`.
2. Whether `FInstanceBufferView` stores raw `ID3D11Buffer*` or a small wrapper type.
   - Suggested for phase 1: raw non-owning `ID3D11Buffer*`, count, stride, offset.
3. Whether particle proxy owns one shared sprite instance buffer per component or one buffer per emitter.
   - Suggested for phase 1: one reusable dynamic buffer per particle component proxy, filled per emitted command.
4. Whether to keep the old particle builder case as a debug fallback during initial rollout.
   - Suggested: keep temporarily until build and visual validation pass, then remove or hard-disable to avoid double submission.
5. Whether proxy collection should receive `ID3D11DeviceContext*` directly.
   - Suggested: yes for this engine phase, because no render thread exists and existing code already performs dynamic uploads during render collection/draw preparation.

## Verification

- `ReleaseBuild.bat` succeeds.
- Existing sprite particle sample renders.
- Particle reset/template change does not crash.
- No double submission through both proxy and legacy particle builder.
- `FParticleRenderPass` no longer owns sprite instance buffers.
