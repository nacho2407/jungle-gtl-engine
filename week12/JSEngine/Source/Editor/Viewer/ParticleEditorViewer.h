#pragma once

#include "Editor/Viewer/EditorViewer.h"

#include "Particle/ParticleAsset.h"
#include "Particle/ParticleSystemComponent.h"

class AActor;
class UParticleSystem;
class UParticleSystemComponent;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;

class FParticleEditorViewer : public FEditorViewer
{
public:
	bool ChangeTarget(const FString& InFileName) override;
	EEditorTabKind GetTabKind() const override;
	const char* GetViewerLabel() const override;

	FEditorViewportClient& GetClient() override { return Client; }
	const FEditorViewportClient& GetClient() const override { return Client; }

	void Tick(float DeltaTime) override;
	void Shutdown() override;

	UParticleSystem* GetParticleSystem() const { return ParticleSystem; }
	UParticleSystemComponent* GetPreviewComponent() const { return PreviewComponent; }

	// Selection Controls
	int32 GetSelectedEmitterIndex() const { return SelectedEmitterIndex; }
	int32 GetSelectedLODIndex() const { return SelectedLODIndex; }
	int32 GetSelectedModuleIndex() const { return SelectedModuleIndex; }

	void SelectEmitter(int32 EmitterIndex);
	void SelectLOD(int32 LODIndex);
	void SelectModule(int32 ModuleIndex);

	UParticleEmitter* GetSelectedEmitter() const;
	UParticleLODLevel* GetSelectedLODLevel() const;
	UParticleModule* GetSelectedModule() const;

	UObject* GetSelectedObject() const;

	// Simulation Controls
	void RestartSimulation();
	void SetPlaying(bool bInPlaying) { bPlaying = bInPlaying; }
	bool IsPlaying() const { return bPlaying; }

	void SetLooping(bool bInLooping) { bLooping = bInLooping; }
	bool IsLooping() const { return bLooping; }

	// Emitter Panels
	void AddEmitter();
	void DeleteSelectedEmitter();

	void AddModule(UClass* ModuleClass);
	void DeleteSelectedModule();

	// Toolbar Actions
	bool Save();
	void AddLOD();
	void RemoveLOD(int32 LODIndex);
	void SetHighestLOD();
	void SetLowestLOD();
	void MoveToLowerLOD();
	void MoveToUpperLOD() { }

	void MarkDirty() { bDirty = true; }
	bool IsDirty() const { return bDirty; }

private:
	void ClearParticlePreview();
	void ClearParticleSelection();

	bool CreatePreviewComponent();

	bool LoadParticleSystemAsset(const FString& InFileName);
	void EnsureDefaultParticleSystem();

private:
	FEditorViewportClient Client;
	UParticleSystem* ParticleSystem = nullptr;
	UParticleSystemComponent* PreviewComponent = nullptr;
	AActor* PreviewActor = nullptr;
	// AParticleActor* PreviewActor = nullptr;

	int32 SelectedEmitterIndex = -1;
	int32 SelectedLODIndex = -1;
	int32 SelectedModuleIndex = -1;

	bool bPlaying = true;
	bool bLooping = true;
	bool bDirty = false;
};
