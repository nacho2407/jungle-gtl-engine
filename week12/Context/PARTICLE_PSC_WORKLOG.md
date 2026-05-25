# Particle PSC Worklog

## 2026-05-25 - Initial checkpoint and worklog setup

- Branch / HEAD: `feat/PSC` / `2de8e38`
- Completed step: Confirmed the current post-merge particle structure and established the staged worklog workflow.
- Changed files: `AGENTS.md`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: Prior baseline verification confirmed `JSEngine.sln` builds successfully for `Debug|x64`; this documentation-only step does not require a rebuild. Document contents were reviewed and whitespace checks reported no errors.
- Repository note: The existing `.gitignore` excludes `AGENTS.md`, so the new instructions file is available to local Codex sessions but is not shown as a normal untracked repository file unless the ignore policy is changed or the file is deliberately force-added.
- Findings: Particle Core now provides `FParticleDataContainer`, `ParticleIndices`, `GetActiveParticleCount()`, `GetParticleByActiveIndex()`, `Init()`, and per-LOD runtime caches, so Mesh render snapshot packing can rely on public Core APIs.
- Findings: `UParticleSystemComponent` is already spawnable as an Actor component, but it has no serialized ParticleSystem asset reference or loading connection point.
- Findings: `FResourceManager` has no `LoadParticleSystem()` API yet; PSC asset resolution must remain an explicit TODO connection point for the asset owner.
- Deferred TODO: Connect PSC soft ParticleSystem reference after the ParticleSystem asset loader contract exists.
- Deferred TODO: Connect Mesh TypeData StaticMesh reference serialization to the Particle asset serializer.
- Deferred TODO: Leave Mesh render snapshot consumption and GPU draw integration to the particle rendering owner.
- Deferred TODO: Validate non-empty mesh instance output once Core spawn/runtime activation is available through the merged lifecycle.
- Next step: Add the minimal `UParticleModuleTypeDataMesh` and `FDynamicMeshEmitterData` StaticMesh/snapshot contract only after approval.

## 2026-05-25 - Mesh TypeData and render snapshot contract

- Branch / HEAD: `feat/PSC` / `2de8e38`
- Completed step: Added the minimum StaticMesh reference contract for Mesh TypeData and storage slots required by a future mesh render snapshot.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleModules.h`, `JSEngine/Source/Engine/Particle/ParticleModules.cpp`, `JSEngine/Source/Engine/Particle/ParticleTypes.h`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; reflection generation registers `UParticleModuleTypeDataMesh::MeshAssetPath` as `TSoftObjectPtr<UStaticMesh>`; `git diff --check` reported no whitespace errors.
- Added contract: `UParticleModuleTypeDataMesh` exposes `SetStaticMesh()` / `GetStaticMesh()`, keeps a runtime non-owning `UStaticMesh*`, and stores an editable `MeshAssetPath` soft reference.
- Added contract: `FDynamicMeshEmitterData` now has a non-owning `UStaticMesh* Mesh` and `TArray<FMeshParticleInstanceVertex> InstanceVertices` for the render snapshot boundary.
- Deferred TODO: Resolve `MeshAssetPath` during ParticleSystem asset deserialization once the asset-owner loading contract is available.
- Deferred TODO: Override `UParticleModuleTypeDataMesh::CreateInstance()` and `GetDynamicRenderData()` to produce mesh instances and snapshots.
- Deferred TODO: Populate `InstanceVertices` through Core public active-particle APIs without modifying the default Sprite path.
- Deferred TODO: Leave consumption of `Mesh` and `InstanceVertices` by GPU rendering code to the particle rendering owner.
- Next step: Implement Mesh TypeData runtime dispatch (`CreateInstance()` / `GetDynamicRenderData()`) and fill the CPU snapshot from active particles after approval.

## 2026-05-25 - Mesh emitter runtime dispatch and CPU snapshot packing

- Branch / HEAD: `feat/PSC` / `4b4328b`
- Completed step: Implemented Mesh TypeData runtime dispatch and CPU-side instance snapshot packing.
- Changed files: `AGENTS.md` (local ignored instruction file), `JSEngine/Source/Engine/Particle/ParticleModules.h`, `JSEngine/Source/Engine/Particle/ParticleModules.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors.
- Added contract: Newly written code comments and `TODO` comments must be written in Korean.
- Added behavior: `UParticleModuleTypeDataMesh::CreateInstance()` creates `FParticleMeshEmitterInstance`.
- Added behavior: `GetDynamicRenderData()` returns `FDynamicMeshEmitterData` only when a StaticMesh is assigned, copies the mesh reference and sort metadata, and packs each active Core particle into `InstanceVertices`.
- Added behavior: Local-space mesh particles compose their per-particle scale/translation transform with the component-to-world transform; world-space particles keep their simulation-space transform.
- Deferred TODO: Apply `Particle.Rotation` after the mesh particle rotation-axis/alignment policy is agreed.
- Deferred TODO: Resolve `MeshAssetPath` during ParticleSystem asset deserialization once the asset-owner loading contract is available.
- Deferred TODO: Leave consumption of the CPU mesh snapshot by GPU rendering code to the particle rendering owner.
- Next step: Prepare `UParticleSystemComponent` ParticleSystem asset-reference persistence with an explicit unresolved loader TODO, after approval.

## 2026-05-25 - PSC ParticleSystem asset reference persistence scaffold

- Branch / HEAD: `feat/PSC` / `47c904d`
- Completed step: Added the Scene-persisted ParticleSystem soft reference field and a single unresolved runtime resolution point on `UParticleSystemComponent`.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.h`, `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; reflection generation registers `UParticleSystemComponent::TemplateAssetPath` as `TSoftObjectPtr<UParticleSystem>`; `git diff --check` reported no whitespace errors.
- Added contract: `TemplateAssetPath` is reflected and therefore preserved through component Scene serialization without duplicating the ParticleSystem asset graph.
- Added behavior: PSC overrides `Serialize()` and `PostEditProperty()` so loaded or edited references enter `ResolveTemplateAssetReference()` consistently.
- Added behavior: An empty path clears the runtime template and emitter instances; a non-empty unresolved path also clears stale runtime data until the asset loader is connected.
- Deferred TODO: `ParticleSystemAssetLoader.cpp` is currently empty, so `ResolveTemplateAssetReference()` must be wired to the completed Asset loader rather than introducing a competing load policy here.
- Deferred TODO: Provide path synchronization when the Asset-owned ParticleSystem runtime object exposes its persistent path contract.
- Next step: Implement distance-based `UParticleLODLevel` selection only after approval and after rechecking the latest Core LOD API.

## 2026-05-25 - Post-merge Particle Core API and baseline build check

- Branch / HEAD: `feat/PSC` / `999f3db`
- Completed step: Inspected the merged Particle Core lifecycle/LOD public API and ran the required pre-extension `Debug|x64` baseline build.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` `Debug|x64` build failed before LOD changes with `ParticleSystemComponent.h(61,5): error C7568` at `TSoftObjectPtr<UParticleSystem> TemplateAssetPath`.
- Findings: Core now owns LOD memory initialization through `FParticleEmitterInstance::Init(EmitterTemplate, LODIndex)`, which selects `CurrentRuntimeCache`, allocates the selected stride/payload layout, and calls `Reset()`.
- Findings: The safe initial LOD transition remains a component-controlled emitter recreation or `Init()` reinitialization path rather than direct mutation of instance memory layout fields.
- Blocker: `UParticleSystemComponent` declares `TSoftObjectPtr` without directly including `Object/ObjectPtr.h`; after the merge it no longer compiles through incidental include availability.
- Remaining TODO: Add the missing PSC header dependency and rerun the baseline build before implementing distance-based LOD selection.
- Remaining TODO: Implement distance-based LOD selection with reinitialization on a selected LOD change after the baseline build is clean.
- Next step: Restore the merged baseline build by adding the PSC `TSoftObjectPtr` header dependency, after approval.

## 2026-05-25 - PSC soft ParticleSystem property build restoration

- Branch / HEAD: `feat/PSC` / `999f3db`
- Completed step: Restored the post-merge PSC baseline build by declaring the direct dependencies required by the reflected `TSoftObjectPtr<UParticleSystem>` property.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.h`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors.
- Added dependency: `ParticleSystemComponent.h` now includes `Object/ObjectPtr.h` for `TSoftObjectPtr` and `Particle/ParticleAsset.h` so generated reflection code can call `UParticleSystem::StaticClass()`.
- Remaining TODO: Implement distance-based LOD selection with safe emitter reinitialization when the selected LOD changes.
- Remaining TODO: Keep ParticleSystem asset resolution limited to the existing Asset-loader TODO until the loader contract is provided.
- Next step: Implement distance-based `UParticleLODLevel` selection through PSC-managed reinitialization, after approval.

## 2026-05-25 - PSC distance-based LOD selection and safe transition

- Branch / HEAD: `feat/PSC` / `35da487`
- Completed step: Implemented component-managed distance-based LOD selection and safe emitter recreation when the active LOD changes.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.h`, `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; reflection generation registers `UParticleSystemComponent::LODDistanceInterval`; `git diff --check` reported no whitespace errors.
- Added behavior: PSC exposes a Scene-persisted `LOD Distance Interval` and selects each emitter LOD using the distance between the component and the world's active camera.
- Added behavior: A missing active camera, a non-positive interval, or an emitter with one LOD keeps LOD 0 active.
- Added behavior: When a selected LOD changes, PSC releases render snapshots and recreates emitter instances through `TypeDataModule->CreateInstance()` followed by `Instance->Init(EmitterTemplate, LODIndex)`, so LOD-specific TypeData and payload layouts are re-established by Core.
- Deferred TODO: Replace the uniform PSC distance interval with ParticleSystem asset-owned LOD distance settings once the Asset contract provides them.
- Deferred TODO: Exercise a populated multi-LOD ParticleSystem in runtime/editor preview once a loadable ParticleSystem template path is connected or a test template fixture is available.
- Next step: Validate runtime LOD switching with populated LOD templates after a usable ParticleSystem template source is available, or proceed with another approved PSC/Mesh task.

## 2026-05-25 - Post-Core integration follow-up scope analysis

- Branch / HEAD: `feat/PSC` / `27a6570`
- Completed step: Reviewed the currently merged Particle Core surface and adjacent StaticMesh/Particle Viewer integration points to identify additional PSC/Mesh work that does not cross Asset or Renderer ownership.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: Analysis only; inspected `main` at `467c630`, current PSC head, StaticMesh loading/component patterns, Particle Viewer template creation, and Core emitter runtime behavior.
- Findings: `UParticleModuleTypeDataMesh::MeshAssetPath` is reflected but no code resolves an edited path into its runtime `UStaticMesh* Mesh`; `SetStaticMesh()` currently has no caller in the particle path, so mesh snapshot generation remains unreachable through the existing Particle Viewer editing flow.
- Findings: `UStaticMeshComponent` already establishes the acceptable narrow runtime resolution pattern through `FResourceManager::Get().LoadStaticMesh()`, so Mesh TypeData can add an equivalent property-edit resolution point without defining ParticleSystem asset serialization policy.
- Findings: Particle Viewer supplies an in-memory `UParticleSystem` template through `PreviewComponent->SetTemplate()`, so PSC/LOD control paths can be exercised there once runtime particles are produced.
- Blocked boundary: Current Core `FParticleEmitterInstance::Tick()` updates modules only and does not activate/spawn particles, so validating non-empty `FDynamicMeshEmitterData::InstanceVertices` or visual LOD transitions remains blocked by Core runtime progress.
- Deferred TODO: Leave ParticleSystem asset loading/serialization and asset-owned LOD distance data to the Asset owner.
- Deferred TODO: Leave mesh snapshot draw consumption and GPU renderer wiring to the Renderer owner.
- Next step: Add `UParticleModuleTypeDataMesh` runtime StaticMesh resolution for `MeshAssetPath` edits using the existing StaticMesh resource-loading contract, after approval.

## 2026-05-25 - Mesh snapshot-backed PSC bounds and raycast

- Branch / HEAD: `feat/PSC` / `27a6570`
- Completed step: Replaced the placeholder PSC bounds and picking behavior with Mesh render snapshot-backed calculations.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors.
- Added behavior: `UpdateWorldAABB()` now merges each mesh particle instance's transformed `UStaticMesh` local bounds from `FDynamicMeshEmitterData`, instead of exposing a fixed unit box.
- Added behavior: `RaycastMesh()` now intersects rays against the same snapshotted mesh instances, uses per-instance transformed mesh geometry, and returns the closest world-space hit result.
- Boundary: Bounds and picking intentionally consume the mesh render snapshot rather than traversing through Mesh TypeData, matching the renderer-facing snapshot ownership rule.
- Blocked boundary: `AParticleEventManager` cannot be usefully implemented from PSC alone because Core currently exposes queue callbacks but does not emit spawn, death, collision, or burst events, and no event dispatch contract is supplied for the manager.
- Remaining TODO: Add `UParticleModuleTypeDataMesh` runtime StaticMesh resolution for `MeshAssetPath` edits so mesh snapshot creation is reachable from the existing editor path.
- Remaining TODO: Implement or integrate event production only through the Particle Core-owned lifecycle once its spawning, death, and collision behavior is available.
- Next step: Add Mesh TypeData runtime StaticMesh resolution for editable `MeshAssetPath`, after approval.

## 2026-05-25 - LOD distance contract correction analysis

- Branch / HEAD: `feat/PSC` / `27a6570`
- Completed step: Compared the current PSC interval-based LOD selection with the bundled Cascade reference implementation and corrected the design conclusion.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: Analysis only; inspected Cascade uses of `ParticleSystem->LODDistances`, including initialization, insertion, copying, and editor `SetLODLevel()` control.
- Findings: Cascade stores customizable per-LOD thresholds on `UParticleSystem::LODDistances`; the `LODIndex * 2500.0f` expression is only a generated default value, not the runtime data model.
- Findings: The current `UParticleSystemComponent::LODDistanceInterval` supports only evenly spaced automatic selection and therefore is an interim fallback, not a sufficient Unreal-style LOD authoring contract.
- Boundary: Introducing persisted `UParticleSystem` LOD distance fields or serialization from PSC would define Asset-owned policy, which remains outside this task until the Asset owner supplies or approves that contract.
- Candidate within PSC scope: Once an Asset-owned `LODDistances` public API exists, update automatic selection to consume its per-LOD thresholds and expose a component-side/manual LOD override path for editor preview and runtime forcing.
- Remaining TODO: Replace or demote `LODDistanceInterval` after the ParticleSystem LOD distance contract is supplied.
- Next step: Confirm with the Asset owner whether `UParticleSystem::LODDistances` and LOD selection mode are being added to the asset contract; continue independent Mesh TypeData runtime resolution work in the meantime, after approval.

## 2026-05-25 - PSC soft ParticleSystem loader compatibility analysis

- Branch / HEAD: `feat/PSC` / `27a6570`
- Completed step: Verified the intended handoff between the Scene-persisted PSC soft reference and the Asset-owned ParticleSystem loader path.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: Analysis only; inspected `TSoftObjectPtr` property serialization, existing StaticMesh/SkeletalMesh component resolution patterns, `FParticleSystemAssetLoader`, `FResourceManager`, and the reflected Particle asset classes.
- Findings: `UParticleSystemComponent::TemplateAssetPath` is compatible as the Scene-side reference contract because reflection stores only its asset path while runtime `Template` is resolved separately, matching existing mesh component design.
- Findings: The handoff is not connected yet: `FParticleSystemAssetLoader` declares `Load()`/`Save()` but its implementation file is empty, `FResourceManager` exposes no `LoadParticleSystem()` API, and PSC therefore correctly stops at `ResolveTemplateAssetReference()`'s Asset integration TODO.
- Findings: `UParticleSystem::Emitters`, `UParticleEmitter::LODLevels`, and `UParticleLODLevel` module slots/arrays currently have no reflected properties, so the loader header's reflection-unification TODO cannot persist the asset graph without additional Asset-side reflection work or explicit serialization.
- Required Asset handoff: The Asset owner should serialize the ParticleSystem graph plus `UParticleSystem::LODDistances`, publish a stable `LoadParticleSystem(Path)` resolution route (preferably through `FResourceManager`), and expose a persistent asset path on loaded ParticleSystem objects if PSC editor assignment must synchronize its soft reference.
- Boundary: PSC must serialize only `TemplateAssetPath` and later call the public asset resolution route from `ResolveTemplateAssetReference()`; it must not serialize nested ParticleSystem contents or define the loader format.
- Remaining TODO: Connect `ResolveTemplateAssetReference()` to the public ParticleSystem loading API once supplied, and replace the interim component `LODDistanceInterval` with asset-owned `LODDistances` consumption.
- Next step: Share the required Asset handoff contract with the Asset owner; continue an independently approved PSC/Mesh task while that API is being implemented.

## 2026-05-25 - PSC bounds and raycast partial-implementation rollback

- Branch / HEAD: `feat/PSC` / `27a6570`
- Completed step: Removed the Mesh-only implementation from PSC-wide bounds and raycast entry points after identifying that it silently omitted Sprite and mixed-emitter systems.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors.
- Correction: `UpdateWorldAABB()` now resets the unsupported PSC bound rather than returning either an arbitrary unit box or incomplete Mesh-only bounds.
- Correction: `RaycastMesh()` now remains unsupported rather than returning hits only for Mesh emitter snapshots through a component-wide API.
- Boundary: Accurate PSC bounds and picking require a contract covering every renderable emitter snapshot type; implementing only Mesh behavior here is not valid for the component.
- Remaining TODO: Implement PSC-wide bounds/picking only once Sprite and Mesh snapshot contracts can both be consumed without modifying renderer-owned draw behavior.
- Next step: Continue an independently approved PSC/Mesh integration step after this rollback is verified.

## 2026-05-25 - Particle event manager preimplementation analysis

- Branch / HEAD: `feat/PSC` / `baf1f62`
- Completed step: Determined which `AParticleEventManager` behavior can be implemented before Core and Asset event contracts are available.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: Analysis only; inspected PSC event queues/finalization, `IParticleEmitterInstanceOwner`, Core tick/module behavior, particle event data structs, the engine delegate pattern, and the bundled Cascade event-generator reference.
- Findings: PSC already queues and forwards spawn, death, collision, and burst arrays to `AParticleEventManager`, but no code assigns an event manager to a PSC and Core does not invoke any `Add*Event()` path because particle spawning, death, and collision generation are not implemented yet.
- Findings: Cascade event behavior depends on an asset-side `EventGenerator`/receiver module model; the current Particle asset graph and modules contain neither contract, and the current event structs do not include event names or receiver-selection metadata.
- Safe candidate: A custom engine-level observer surface can be added now by exposing four `TDelegate` broadcasts from `AParticleEventManager::HandleParticle*Events()`; it would make manually injected or future Core-produced events observable without deciding how events spawn other particle effects.
- Boundary: Implementing Cascade-style event receiver routing, effect spawning, collision consequences, or manager auto-registration now would invent Core/Asset/runtime policy outside the available contract.
- Remaining TODO: Decide whether this project wants the narrow delegate observer surface as an interim/public runtime API, or will wait for the Asset/Core-owned Cascade event module contract.
- Next step: If the delegate observer API is approved, add broadcasts and a minimal injection/usage verification step; otherwise leave `AParticleEventManager` as a TODO sink until generator/receiver APIs arrive.

## 2026-05-25 - PSC particle event delegate dispatch scaffold

- Branch / HEAD: `feat/PSC` / `baf1f62`
- Completed step: Added the Cascade-aligned component-owned particle event delegate surface and filled `AParticleEventManager` as its forwarding dispatcher.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.h`, `JSEngine/Source/Engine/Particle/ParticleEventManager.h`, `JSEngine/Source/Engine/Particle/ParticleEventManager.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors.
- Added behavior: PSC now exposes `OnParticleSpawn`, `OnParticleDeath`, `OnParticleCollide`, and `OnParticleBurst` delegate endpoints, following Unreal Cascade's ownership of particle event listeners on `UParticleSystemComponent`.
- Added behavior: `AParticleEventManager::HandleParticle*Events()` methods broadcast each received event item to the corresponding PSC delegate.
- Constraint: The current local event structs do not yet contain Unreal-level event name, emitter time, velocity, or receiver metadata; delegate payloads intentionally expose only the existing event records until Core/Asset extends the contract.
- Boundary: No automatic event manager creation/assignment, event generation, receiver routing, or spawned-effect policy was added.
- Correction: The handler methods remain non-virtual because this engine has no current `AParticleEventManager` subclass or override contract; Unreal API virtuality alone is not a reason to publish that extension point locally.
- Remaining TODO: Core must produce spawn/death/collision/burst records and the Asset contract must supply generator/receiver policy before this path can drive authored Cascade-style effects.
- Next step: Continue with another independently approved PSC/Mesh integration step or integrate richer event payloads once Core/Asset publishes that contract.

## 2026-05-25 - PSC event queue reporting and lazy manager connection

- Branch / HEAD: `feat/PSC` / `baf1f62`
- Completed step: Opened explicit PSC event reporting entry points and connected queued events to a shared, non-scene `AParticleEventManager` created lazily by the owning world.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.h`, `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`, `JSEngine/Source/Engine/GameFramework/World.h`, `JSEngine/Source/Engine/GameFramework/World.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors.
- Added behavior: The externally declared `FParticle*Signature` delegate aliases compile after forward-declaring `UParticleSystemComponent`, keeping the event signature types reusable outside the component declaration.
- Added behavior: PSC now exposes `ReportEventSpawn()`, `ReportEventDeath()`, `ReportEventCollision()`, and `ReportEventBurst()` as the queue insertion contract; the instance owner forwards future Core event records through these methods.
- Added behavior: If queued records exist and no custom manager was assigned, `FinalizeTickComponent()` asks `UWorld` for one shared lazy `AParticleEventManager` and dispatches through it.
- Ownership boundary: The lazy manager is held and destroyed by `UWorld` without insertion into `ULevel::Actors`, so merely reporting particle events does not add a serializable scene actor.
- Boundary: This does not invent particle lifecycle production; Core must still call the existing `IParticleEmitterInstanceOwner::Add*Event()` hooks at real spawn/death/collision/burst points.
- Remaining TODO: Replace the temporary world-owned manager location if an engine-wide or WorldSettings-owned Particle Event Manager policy is introduced.
- Next step: Hand the `ReportEvent*()`/`IParticleEmitterInstanceOwner::Add*Event()` production hook contract to Core, or proceed with another approved PSC/Mesh integration step.

## 2026-05-25 - Post-Core Mesh TypeData runtime resolution and snapshot access

- Branch / HEAD: `feat/PSC` / `5828aba` (uncommitted step changes)
- Completed step: Connected editable Mesh TypeData soft references to runtime StaticMesh resolution and exposed a read-only PSC render snapshot inspection boundary.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleModules.h`, `JSEngine/Source/Engine/Particle/ParticleModules.cpp`, `JSEngine/Source/Engine/Particle/ParticleSystemComponent.h`, `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors.
- Added behavior: `UParticleModuleTypeDataMesh::PostEditProperty()` resolves `MeshAssetPath` through `FResourceManager::LoadStaticMesh()` using the established StaticMesh component runtime pattern; an empty or unresolved path clears the runtime mesh.
- Added contract: PSC exposes snapshot count and per-index `const FDynamicEmitterDataBase*` lookup so integration tests and future renderer-owned consumption can inspect packed data without mutating PSC-owned storage.
- Boundary: This step does not add Particle Viewer TypeData editing UI, ParticleSystem asset deserialization, or renderer draw/GPU snapshot consumption.
- Remaining TODO: Replace PSC interval-based LOD selection with `UParticleSystem::LODDistances` consumption and validate Core-produced spawn/death/burst forwarding.
- Remaining TODO: Add the approved `particle test` runtime self-test only after the LOD/event integration step is completed.
- Next step: Implement Asset-owned LOD threshold selection and lifecycle event integration verification, after approval.

## 2026-05-25 - Asset-owned LOD selection and lifecycle event integration check

- Branch / HEAD: `feat/PSC` / `5828aba` (uncommitted step changes)
- Completed step: Replaced PSC-local interval selection with ParticleSystem-owned LOD thresholds and verified the merged Core event production path still reaches PSC dispatch.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleSystemComponent.h`, `JSEngine/Source/Engine/Particle/ParticleSystemComponent.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors; static inspection confirmed Core spawn/death/burst calls flow through `IParticleEmitterInstanceOwner`, PSC queues, and `AParticleEventManager` broadcasts.
- Added behavior: Automatic LOD selection consumes `UParticleSystem::LODDistances`; a missing camera, missing next threshold, or single-LOD emitter remains at LOD 0.
- Added behavior: Selection can advance only through available emitter/threshold pairs, and stops at a negative or decreasing subsequent threshold so malformed asset data cannot cause further transitions.
- Removed interim contract: `UParticleSystemComponent::LODDistanceInterval` is no longer exposed or consumed now that the asset-owned distance surface exists.
- Boundary: This step does not add LOD authoring UI or serialization policy, lifecycle receiver routing, or collision-event generation.
- Remaining TODO: Keep ParticleSystem asset resolution at the existing Asset-loader TODO until the public loader contract is supplied.
- Remaining TODO: Add and run the approved `particle test` console self-test for Mesh snapshot, LOD transition, and spawn/death/burst dispatch behavior.
- Next step: Implement the runtime console self-test fixture and command, after approval.

## 2026-05-25 - Mesh TypeData property callback review

- Branch / HEAD: `feat/PSC` / `5828aba` (uncommitted step changes)
- Completed step: Reviewed the current `UParticleModuleTypeDataMesh::PostEditProperty()` callback shape against the existing StaticMesh component property-resolution pattern.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: Analysis only; inspected the current Mesh TypeData implementation, `UObject::PostEditProperty()` contract, and `UStaticMeshComponent::PostEditProperty()`.
- Findings: Resolving `MeshAssetPath` in `UParticleModuleTypeDataMesh::PostEditProperty()` is the correct runtime editing boundary and does not define ParticleSystem asset serialization policy.
- Findings: The current conditional `if (PropertyName || std::strcmp(PropertyName, "MeshAssetPath") == 0)` is invalid because any non-null property name enters the reload path and a null property name reaches `std::strcmp()` with a null pointer.
- Remaining TODO: Restore the guarded `MeshAssetPath` equality check before relying on this runtime resolution path.
- Next step: Correct the Mesh TypeData property-name guard after approval, then resume the approved runtime self-test step.
