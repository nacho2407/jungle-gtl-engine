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

## 2026-05-25 - Runtime particle self-test command and execution

- Branch / HEAD: `feat/PSC` / `7f092ff` (uncommitted step changes)
- Completed step: Added the disposable Editor console `particle test` self-test, corrected the Mesh TypeData property guard required by that path, and executed runtime verification for Mesh snapshots, LOD selection, and lifecycle event dispatch.
- Changed files: `JSEngine/Source/Editor/UI/EditorConsoleWidget.h`, `JSEngine/Source/Editor/UI/EditorConsoleWidget.cpp`, `JSEngine/Source/Engine/Particle/ParticleModules.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors; a temporary one-run launch hook was used and then removed after `Editor.log` reported `[ParticleTest] completed: 14 passed, 0 failed`.
- Added verification path: `particle test` constructs in-memory PSC fixtures and reports Mesh path resolution, packed Mesh snapshot count/data, near/far/fallback LOD selection, and spawn/burst/death delegate dispatch from real ticks.
- Correction: `UParticleModuleTypeDataMesh::PostEditProperty()` now resolves only non-null `MeshAssetPath` property edits; the test fixture uses the loadable `Asset\Mesh\Lumine\Lumine_Praise.obj` path because the previously referenced Dice OBJ is not present.
- Boundary: The self-test intentionally does not cover collision event production, Sprite renderer consumption, or ParticleSystem asset serialization/loading; only the manual console command remains after verification.
- Remaining TODO: Particle Viewer Mesh/LOD authoring UI and asset-owned serialization remain external integration work; renderer snapshot consumption remains renderer-owned.
- Next step: Discard the temporary console self-test code after the requested investigation, or retain it for further manual regression checks.

## 2026-05-25 - Mesh snapshot alignment with Core sprite snapshot contract

- Branch / HEAD: `feat/PSC` / `c62b827` (uncommitted step changes)
- Completed step: Aligned `UParticleModuleTypeDataMesh::GetDynamicRenderData()` eligibility checks and replay metadata fallback with the newly merged Core Sprite snapshot implementation.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleModules.cpp`, `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `JSEngine.sln` builds successfully for `Debug|x64`; `git diff --check` reported no whitespace errors. The build included an existing uncommitted `JSEngine/Source/Engine/Particle/ParticleTypes.h` member-order change that was not modified by this step.
- Added behavior: Mesh snapshot creation now requires a resolved Mesh, active particles, valid simulation particle/index memory, a positive simulation stride, and an initialized runtime cache; empty or invalid emitter state returns no render snapshot.
- Preserved contract: Mesh snapshots continue to carry `UStaticMesh*` and `InstanceVertices` only for per-particle rendering, with instance stride metadata and existing local-space component transform application; Sprite packed `DataContainer` is not reused for Mesh.
- Added behavior: Mesh replay sort mode now explicitly falls back to `ViewDepthBackToFront` when no Required module is available, matching the Base snapshot default.
- Remaining TODO: Renderer-owned draw/GPU buffer consumption, Particle Viewer Mesh/LOD authoring, and ParticleSystem asset serialization/loading remain outside PSC ownership.
- Next step: Hand off the Mesh snapshot for renderer consumption or proceed with a separately approved PSC integration step.

## 2026-05-25 - Mesh render snapshot field-completeness audit

- Branch / HEAD: `feat/PSC` / `c62b827` (uncommitted step changes)
- Completed step: Audited the Mesh snapshot fields against the merged Sprite snapshot contract, PSC ownership, and available renderer-facing types without changing production code or existing comments.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: Static inspection confirmed PSC assigns `EmitterIndex`; `FDynamicMeshEmitterReplayDataBase` constructs the Mesh emitter type; Mesh packing assigns `Mesh`, instance vertex transforms/colors, `ActiveParticleCount`, instance vertex stride, and `SortMode`.
- Finding: No required Mesh-owned render data is omitted under the current public contract. Sprite `DataContainer` holds copied simulation-stride particle memory, while Mesh render state is carried by `FDynamicMeshEmitterData::Mesh` and `InstanceVertices`.
- Boundary: `Rotation` application remains the existing unresolved Mesh alignment TODO; the renderer still owns eventual consumption of Mesh snapshot data.
- Remaining TODO: Do not populate Sprite-style `DataContainer` for Mesh unless the renderer owner publishes a new shared consumption contract.
- Next step: Continue with renderer handoff or another separately approved PSC task.

## 2026-05-25 - Current completion status audit

- Branch / HEAD: `feat/PSC` / `2d62a67` (`ParticleModules.cpp`, `ParticleTypes.h`에 기존 미커밋 변경 존재)
- Completed step: 이전 작업 로그, 현재 소스/미커밋 diff, Asset/Renderer 연결 지점을 대조하여 PSC/Mesh 작업의 완료 여부를 재점검했다.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md` (이 분석 단계만의 변경); 기존 미커밋 `JSEngine/Source/Engine/Particle/ParticleModules.cpp`, `JSEngine/Source/Engine/Particle/ParticleTypes.h`는 수정하지 않았다.
- Verification: 현재 작업 트리를 포함한 `JSEngine.sln` `Debug|x64` 빌드가 성공했고, `git diff --check`는 whitespace 오류를 보고하지 않았다. `LoadParticleSystem`, Mesh snapshot 소비, `particle test` 잔존 여부를 정적 검색했다.
- Conclusion: 현재 상태를 전체 완료로 승인할 수 없다. 커밋된 PSC의 soft reference scaffold, Asset-owned `LODDistances` 선택, event forwarding, Mesh path 편집 해석 경로는 존재하고 빌드되지만, 아래 계약 충돌과 외부 연결 TODO가 남아 있다.
- Finding: `HEAD`의 Mesh snapshot은 `UStaticMesh* Mesh`와 `InstanceVertices`를 전달했으나, 현재 미커밋 변경은 `InstanceVertices`를 제거하고 simulation-stride `DataContainer`를 전달하여 중앙 renderer가 Mesh instance transform을 만들도록 바꾼다. 이는 Mesh snapshot이 renderer에 필요한 per-particle instance snapshot을 보유해야 한다는 현재 작업 제약 및 직전 감사 결론과 일치하지 않는다.
- Finding: `ResolveTemplateAssetReference()`는 여전히 Asset loader 연결 TODO에서 멈추며, `FParticleSystemAssetLoader` 구현 및 public `LoadParticleSystem` 경로는 검색되지 않았다. 이 미해결 지점은 Asset 소유 경계로 남겨둔 상태다.
- Finding: Renderer 쪽에서 `FDynamicMeshEmitterData`를 소비하는 구현은 검색되지 않았고, 이전 런타임 검증용 `particle test` 명령도 현재 소스에는 남아 있지 않다.
- Remaining TODO: Mesh snapshot 계약을 승인된 `InstanceVertices` 경계로 복원할지, renderer owner와 새 raw snapshot 계약을 명시적으로 승인할지 먼저 결정하고 그 결과를 코드/로그/검증에 일치시킨다.
- Remaining TODO: ParticleSystem asset loader 연결과 renderer draw/GPU 소비는 각 소유자가 공개 계약을 제공한 뒤 연동한다.
- Next step: Mesh snapshot 계약 충돌 해소를 별도 승인 단계로 수행한 뒤 `Debug|x64` 빌드와 가능한 런타임 검증을 다시 실행한다.

## 2026-05-25 - Centralized renderer contract correction

- Branch / HEAD: `feat/PSC` / `2d62a67` (`ParticleModules.cpp`, `ParticleTypes.h`에 기존 미커밋 변경 존재)
- Completed step: `main`의 `69fbe1b` (`Update ParticleTypes.h`) 의도를 확인하고 직전 완료 판정에서 Mesh snapshot 계약을 잘못 해석한 부분을 정정했다.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md` (이 분석 단계만의 변경); 기존 미커밋 production 변경은 수정하지 않았다.
- Verification: `69fbe1b` diff, `3695fce` merge conflict 해소 결과, 현재 Mesh/Sprite snapshot packing 구현과 검색 가능한 renderer 소비 지점을 정적 비교했다. 직전 단계에서 현재 작업 트리의 `JSEngine.sln` `Debug|x64` 빌드는 성공했다.
- Correction: `69fbe1b`는 emitter data의 `Render()` 및 renderer vertex stride 다형성을 제거하고 `FDynamicEmitterReplayDataBase`에 공통 `DataContainer`, `Material`, `ComponentToWorld`, `CoordinateSpace`를 추가하여 중앙 renderer가 emitter type별 vertex/instance 생성을 담당하도록 계약을 변경했다.
- Correction: 따라서 Mesh가 `InstanceVertices`를 보유해야 한다는 직전 분석 결론은 폐기한다. 현재 미커밋 Mesh 변경처럼 `UStaticMesh* Mesh`와 snapshot-owned active particle `DataContainer`를 전달하는 형태도, renderer가 TypeData나 simulation memory를 역참조하지 않는 한 현재 중앙집중형 renderer 경계에 부합한다.
- Scope conclusion: PSC/Mesh 측의 renderer handoff 데이터 준비는 새 공통 replay 계약에 맞추는 방향으로 진행 중이며, 실제 draw/pass/GPU buffer 소비 구현이 검색되지 않는 것은 renderer 소유 작업으로 남겨야 한다. 이 부재만으로 PSC 범위 미완료라고 판정하지 않는다.
- Remaining TODO: 현재 미커밋 Mesh snapshot 변경을 해당 계약의 최종 구현으로 확정하려면 코드 변경을 작업 로그에 정식 반영하고 `Debug|x64` 및 가능한 runtime 검증을 다시 남긴다.
- Remaining TODO: `ResolveTemplateAssetReference()`의 ParticleSystem loader 연결은 여전히 Asset 소유 공개 API가 제공된 뒤 연결할 TODO이다.
- Next step: 승인 후 현재 중앙 renderer용 Mesh snapshot 변경을 검증/정리하거나, Asset loader 공개 API가 제공되면 PSC 참조 해석을 연결한다.

## 2026-05-25 - Assigned deliverables completion audit

- Branch / HEAD: `feat/PSC` / `2d62a67` (`ParticleModules.cpp`, `ParticleTypes.h`에 기존 미커밋 Mesh snapshot 변경 존재)
- Completed step: 담당 목록의 PSC lifecycle, Scene soft reference, Mesh TypeData/runtime/snapshot, LOD 선택 항목을 현재 소스와 중앙 renderer 계약에 맞춰 항목별로 판정했다.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md` (이 분석 단계만의 변경); production 파일은 이 단계에서 수정하지 않았다.
- Verification: `UParticleSystemComponent`, `FParticleEmitterInstance`, `UParticleModuleTypeDataMesh`, reflection 생성 결과, Actor component save/load 경로, Particle Viewer, `69fbe1b` 기반 replay data 계약을 정적 점검했다. 현재 production diff를 포함한 `Debug|x64` 빌드는 직전 점검에서 성공했다.
- Complete in PSC scope: `UParticleSystemComponent`는 reflected spawnable component이며 `SetTemplate()`, emitter instance 생성/해제, tick 및 render snapshot 소유 흐름이 구현되어 있다. 요청의 destroy/reset 동작은 각각 `ReleaseEmitterInstances()`와 `ResetParticles()` 이름으로 제공된다.
- Complete without PSC override: `Activate()`와 `Deactivate()`는 `UActorComponent` 구현이 component tick 활성/비활성을 수행하고 `AActor::Tick()`가 이를 사용하므로, 별도의 PSC override가 요구되는 restart/clear 의미가 추가로 정해지지 않는 한 새 구현은 필요하지 않다.
- Complete narrow persistence contract: `TemplateAssetPath`는 reflected `TSoftObjectPtr<UParticleSystem>`으로 Scene 저장/로드에서 경로를 유지하고, PSC는 load/edit 시 `ResolveTemplateAssetReference()` 연결 지점을 사용한다. 실제 ParticleSystem 객체 로드는 Asset 공개 API 부재로 TODO인 상태가 올바른 경계다.
- Complete in PSC scope: `UParticleModuleTypeDataMesh`는 runtime instance 생성과 편집된 `MeshAssetPath`의 `UStaticMesh` 해석을 제공한다. Mesh snapshot은 중앙 renderer 계약에 따라 `UStaticMesh*`, snapshot-owned particle data/index, material 및 transform-space metadata를 전달하는 방향이 맞다.
- Complete in PSC scope: 거리 기반 선택은 `UParticleSystem::LODDistances`를 사용하고, LOD 변경 시 instance를 다시 생성하여 `Init(..., LODIndex)`가 `CurrentLODLevelIndex`, selected level 및 LOD별 runtime module cache를 함께 교체한다. `UParticleLODLevel::bEnabled`도 emitter tick에서 적용된다.
- Required fix in PSC scope: 현재 미커밋 Mesh snapshot 코드에서 Required module이 없을 때 `ReplayData.CoordinateSpace`를 `World`로 설정하지만, `FParticleEmitterInstance::UsesLocalSpace()`는 같은 조건을 local-space로 판단한다. Required module 없는 Mesh emitter가 particle을 만들면 renderer가 위치 좌표계를 잘못 해석할 수 있으므로 fallback을 runtime 규칙과 일치시켜야 한다.
- Not required in PSC scope: ParticleSystem asset serializer/loader 구현, Particle Viewer의 asset save/load 연결, 중앙 renderer의 Mesh draw/pass/GPU buffer 소비는 각각 Asset/Editor/Renderer 소유 작업이므로 이 목록 완료를 위해 PSC에서 구현하면 안 된다.
- Next step: 승인 후 Mesh snapshot coordinate-space fallback 불일치를 수정하고 현재 중앙 renderer용 snapshot 변경을 `Debug|x64` 빌드로 다시 검증한다.

## 2026-05-25 - Assigned PSC blocker resolution verification

- Branch / HEAD: `feat/PSC` / `2d62a67` (`ParticleModules.cpp`, `ParticleTypes.h`에 미커밋 Mesh snapshot 변경 존재)
- Completed step: Mesh snapshot의 기본 좌표계를 runtime local-space 규칙과 일치시키는 수정 반영 여부를 확인하고 담당 목록의 필수 blocker 해소 여부를 재판정했다.
- Changed files: `JSEngine/Source/Engine/Particle/ParticleModules.cpp` (사용자 수정 확인), `JSEngine/Source/Engine/Particle/ParticleTypes.h` (기존 중앙 renderer snapshot 변경), `Context/PARTICLE_PSC_WORKLOG.md`
- Verification: `ParticleModules.cpp`의 `RequiredModule == nullptr` fallback이 `EParticleCoordinateSpace::Local`로 변경된 것을 확인했고, 현재 작업 트리의 `JSEngine.sln`이 `Debug|x64`로 성공적으로 빌드되었으며 `git diff --check`가 whitespace 오류를 보고하지 않았다.
- Conclusion: 제시된 PSC/Mesh/LOD 담당 기능 중 추가 production 구현이 반드시 필요한 항목은 현재 확인되지 않는다. 중앙 renderer용 Mesh snapshot 변경은 아직 미커밋 상태이므로 제출 전 변경 정리와 커밋은 필요하다.
- Remaining TODO: 새로 추가된 코드 주석 및 TODO 문구가 로컬 작업 규칙의 한국어 작성 요구를 만족하는지 최종 정리한다.
- External TODO: `ResolveTemplateAssetReference()`의 실제 ParticleSystem 로드는 Asset 소유 public loader API가 제공된 뒤 연결한다.
- External TODO: Mesh snapshot의 draw/pass/GPU buffer 소비와 회전/정렬 적용은 중앙 Renderer 소유 구현에서 처리한다.
- Next step: 현재 미커밋 Mesh snapshot 변경과 worklog를 제출 가능한 형태로 정리하여 커밋한다.

## 2026-05-25 - Unreal TypeData Build and final deliverable audit

- Branch / HEAD: `feat/PSC` / `e52f750`
- Completed step: 제공된 Unreal 파티클 참고 소스와 현재 TypeData, PSC, Mesh runtime/render snapshot, LOD 구현을 비교하여 `Build()`의 역할과 최종 잔여 작업을 판정했다.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md` (분석 기록만 추가); 이 단계에서는 production 소스를 수정하지 않았다.
- Verification: 참고 소스의 `ParticleModuleTypeDataBase.h`, `ParticleModuleTypeDataGpu.h`, `ParticleModuleTypeDataMesh.h`, `ParticleEmitter.cpp`, `ParticleLODLevel.cpp`, `ParticleSystemComponent.h/.cpp`를 확인했고, 로컬 `UParticleModuleTypeDataBase::Build()`가 호출되지 않는 빈 구현임을 검색으로 확인했다. 현재 작업 트리의 `JSEngine.sln` `Debug|x64` 빌드는 `UParticleSystemComponent::Activate()` 및 `Deactivate()` 미정의 링크 오류로 실패했다.
- Finding: Unreal의 `UParticleModuleTypeDataBase::Build(FParticleEmitterBuildInfo&)`는 `RequiresBuild()`가 참인 TypeData에만 실행되는 선택적 시뮬레이션 리소스 build hook이다. 참고 소스에서 Mesh TypeData는 이를 override하지 않으며 GPU TypeData가 override한다.
- Finding: 로컬의 인자 없는 `virtual void Build()`는 현재 사용되지 않는 빈 골격이며 Mesh TypeData, Mesh runtime snapshot, PSC/LOD 담당 범위를 완료하기 위해 구현할 필요가 없다. 제거는 선택적인 API 정리에 해당한다.
- Conclusion: Mesh TypeData, Mesh snapshot 및 LOD 범위에는 새 필수 구현 누락이 확인되지 않았지만, 현재 `UParticleSystemComponent` 헤더에 추가된 `Activate()`, `Deactivate()`, `ResetSystem()` 선언은 완료되지 않았다. `Activate()`와 `Deactivate()`는 링크 blocker이며, `ResetSystem()`은 기존 `ResetParticles()` 동작과 연결되어야 명시된 API 요구를 충족한다.
- Remaining TODO: `Activate()` / `Deactivate()` 구현과 `ResetSystem()` 연결을 완료해 링크 오류를 제거하고, 새 주석의 한국어 작성 규칙 준수를 정리한 뒤 `Debug|x64` 빌드를 재실행한다.
- External TODO: `TemplateAssetPath`의 runtime 해석은 Asset 소유 ParticleSystem loader 계약 제공 후 연결하고, Mesh draw 및 회전 소비는 Renderer 소유 구현에서 처리한다.
- Next step: PSC lifecycle 선언의 구현을 완료하는 별도 승인 단계 후 `Debug|x64` 빌드로 재검증한다.

## 2026-05-25 - PSC lifecycle declaration removal and Build override scope verification

- Branch / HEAD: `feat/PSC` / `e52f750`
- Completed step: 불필요한 PSC lifecycle 선언이 제거된 현재 작업 트리를 재검증하고, 제공된 Unreal 참고 소스 전체에서 TypeData `Build()` / `RequiresBuild()` override 범위를 검색했다.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md` (분석 기록만 추가); production 수정은 사용자가 반영한 상태를 확인했다.
- Verification: 현재 `ParticleSystemComponent.h/.cpp`에는 `Activate()`, `Deactivate()`, `ResetSystem()`의 PSC 전용 선언/정의가 없고 base component activation과 기존 reset/release 경로를 사용한다. 현재 작업 트리의 `JSEngine.sln` `Debug|x64` 빌드는 성공했다.
- Finding: 제공된 참고 소스 범위에서 `UParticleModuleTypeDataBase::Build(FParticleEmitterBuildInfo&)`와 `RequiresBuild()`를 override하는 TypeData는 `UParticleModuleTypeDataGpu`만 검색되었다. Mesh TypeData에는 override가 없다.
- Conclusion: PSC lifecycle 전용 API는 현재 요구 동작을 위해 추가할 필요가 없으며, 해당 선언으로 발생했던 링크 blocker는 제거되었다. `UParticleModuleTypeDataBase::Build()` 역시 Mesh 담당 범위의 잔여 구현이 아니다.
- Remaining TODO: 새로 추가된 production 주석/TODO 표현의 한국어 작성 규칙 준수 여부만 제출 전 정리한다.
- External TODO: Asset-owned ParticleSystem loader 연동과 Renderer-owned Mesh draw/rotation 소비는 기존 소유 경계에 남긴다.
- Next step: 주석 규칙 정리 여부를 결정하고 제출 상태를 확정한다.

## 2026-05-25 - Unreal non-GPU emitter TypeData Build coverage check

- Branch / HEAD: `feat/PSC` / `e52f750`
- Completed step: 제공된 Unreal 참고 소스에서 Sprite, Beam2, Ribbon, AnimTrail 경로가 `Build()`를 사용하는지 추가 확인했다.
- Changed files: `Context/PARTICLE_PSC_WORKLOG.md` (분석 기록만 추가); production 소스는 수정하지 않았다.
- Verification: `ParticleEmitter.cpp`, `ParticleModuleTypeDataBase.h`, `ParticleModuleTypeDataGpu.h`, `ParticleModuleTypeDataMesh.h`, `ParticleModuleTypeDataBeam2.h`, `ParticleModuleTypeDataRibbon.h`, `ParticleModuleTypeDataAnimTrail.h`를 검색 및 비교했다.
- Finding: Sprite는 별도 TypeData 클래스가 아니라 `UParticleSpriteEmitter`의 기본 emitter 경로이므로 TypeData `Build()` override 대상이 아니다.
- Finding: Beam2, Ribbon, AnimTrail TypeData는 runtime instance 생성을 override하지만 `Build()` 및 `RequiresBuild()`는 override하지 않는다.
- Conclusion: 제공된 참고 소스 범위에서 TypeData build hook을 활성화하는 유형은 GPU TypeData만 확인되며, Sprite/Mesh/Beam2/Ribbon/AnimTrail의 PSC 및 Mesh 담당 작업에 `Build()` 추가 구현은 필요하지 않다.
- Next step: 남아 있는 주석 규칙 정리만 제출 전 검토한다.
