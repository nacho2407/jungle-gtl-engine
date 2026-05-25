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
