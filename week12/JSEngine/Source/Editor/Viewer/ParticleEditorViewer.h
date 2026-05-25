#pragma once

#include "Editor/Viewer/EditorViewer.h"
#include "Editor/Viewport/Viewer/ParticleViewerViewportClient.h"

#include "Particle/ParticleAsset.h"
#include "Particle/ParticleSystemComponent.h"

class AActor;
class UParticleSystem;
class UParticleSystemComponent;
class UParticleEmitter;
class UParticleLODLevel;
class UParticleModule;
class UParticleModuleRequired;
class UParticleModuleSpawn;
class UParticleModuleTypeDataBase;
class UWorld;

enum class EParticleEditorSelectionType : uint8
{
	None,
	ParticleSystem,
	Emitter,
	LODLevel,
	RequiredModule,
	SpawnModule,
	TypeDataModule,
	Module,
};

// ParticleSystem 에디터 뷰어. ParticleSystem Asset을 로드하여 시뮬레이션, Emitter/LOD/Module 선택 및 편집 기능 제공
// Simulation, Toolbar, Viewport, Details, Emitter Panels 등 UI에서 호출되는 실제 조작 지원
class FParticleEditorViewer : public FEditorViewer
{
public:
	bool ChangeTarget(const FString& InFileName) override;

	void Tick(float DeltaTime) override;
	void Shutdown() override;

	// Client & Preview ──────────────────────────────────────────────────────────────
	FParticleViewerViewportClient& GetClient() override { return Client; }
	const FParticleViewerViewportClient& GetClient() const override { return Client; }

	UParticleSystem* GetParticleSystem() const { return ParticleSystem; }
	UParticleSystemComponent* GetPreviewComponent() const { return PreviewComponent; }

	// Tab Info  ─────────────────────────────────────────────────────────────────────
	EEditorTabKind GetTabKind() const override;
	const char* GetViewerLabel() const override;

	// Selection Controls (Emitter, LOD, Module) ─────────────────────────────────────
	int32 GetSelectedEmitterIndex() const { return SelectedEmitterIndex; }
	int32 GetSelectedLODIndex() const { return SelectedLODIndex; }
	int32 GetSelectedModuleIndex() const { return SelectedModuleIndex; }

	void SelectParticleSystem();
	void SelectEmitter(int32 EmitterIndex);
	void SelectLOD(int32 LODIndex);
	void SelectModule(int32 ModuleIndex);
	void SelectRequiredModule();
	void SelectSpawnModule();
	void SelectTypeDataModule();
	void SelectEmitterModule(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex);

	// Getter of Selected Items ──────────────────────────────────────────────────────
	UParticleEmitter* GetSelectedEmitter() const;
	UParticleLODLevel* GetSelectedLODLevel() const;
	UParticleModule* GetSelectedModule() const;
	UParticleModuleRequired* GetSelectedRequiredModule() const;
	UParticleModuleSpawn* GetSelectedSpawnModule() const;
	UParticleModuleTypeDataBase* GetSelectedTypeDataModule() const;

	UObject* GetSelectedObject() const;
	EParticleEditorSelectionType GetSelectionType() const { return SelectionType; }

	// Simulation Controls ──────────────────────────────────────────────────────────
	void RestartSimulation();
	void RestartLevel();
	void SetPlaying(bool bInPlaying) { bPlaying = bInPlaying; }
	bool IsPlaying() const { return bPlaying; }

	void SetLooping(bool bInLooping) { bLooping = bInLooping; }
	bool IsLooping() const { return bLooping; }

	void SetRealtime(bool bInRealtime);
	bool IsRealtime() const { return Client.IsRealtime(); }

	// View Mode & Display Options ─────────────────────────────────────────────────
	void SetShowGrid(bool bInShowGrid) { Client.SetShowGrid(bInShowGrid); }
	bool IsShowGrid() const { return Client.IsShowGrid(); }

	void SetShowBounds(bool bInShowBounds) { Client.SetShowBounds(bInShowBounds); }
	bool IsShowBounds() const { return Client.IsShowBounds(); }

	void SetBackgroundColor(const FColor& InColor) { Client.SetBackgroundColor(InColor); }
	const FColor& GetBackgroundColor() const { return Client.GetBackgroundColor(); }

	void SetViewMode(EViewMode InViewMode);
	EViewMode GetViewMode() const;

	// Emitter Controls ────────────────────────────────────────────────────────────
	void AddEmitter();
	void DeleteSelectedEmitter();
	void MoveEmitter(int32 FromIndex, int32 ToIndex);

	void AddModule(UClass* ModuleClass);
	void DeleteSelectedModule();
	void MoveModule(int32 FromIndex, int32 ToIndex);
	void MoveModuleToEmitter(int32 ModuleIndex, int32 TargetEmitterIndex);
	void CopyModuleToEmitter(int32 ModuleIndex, int32 TargetEmitterIndex);

	// Toolbar Actions ────────────────────────────────────────────────────────────
	bool Save();
	bool SaveAs(const FString& InFileName);
	void FindInContentBrowser();
	void AddLOD();
	void RemoveLOD(int32 LODIndex);
	void SetHighestLOD();
	void SetLowestLOD();
	void SelectLowerLOD();
	void SelectUpperLOD();

	void MarkDirty() { bDirty = true; }
	bool IsDirty() const { return bDirty; }

private:
	void ClearParticlePreview();
	void ClearParticleSelection();
	bool CreatePreviewComponent();

	bool LoadParticleSystemAsset(const FString& InFileName);
	void EnsureDefaultParticleSystem();
	UParticleLODLevel* CreateDefaultLODLevel(int32 Level);

private:
	FParticleViewerViewportClient Client;
	UParticleSystem* ParticleSystem = nullptr;

	UParticleSystemComponent* PreviewComponent = nullptr;
	AActor* PreviewActor = nullptr;
	UWorld* PreviewWorld = nullptr;
	// AParticleSystemActor* PreviewActor = nullptr;
	bool bOwnsParticleSystem = false; // 뷰어가 생성한 ParticleSystem인지, Asset을 참조한 것인지 구분

	// Selection State
	EParticleEditorSelectionType SelectionType = EParticleEditorSelectionType::None;
	int32 SelectedEmitterIndex = -1;
	int32 SelectedLODIndex = -1;
	int32 SelectedModuleIndex = -1;

	// Simulation & View Options
	bool bPlaying = true;
	bool bLooping = true;
	bool bDirty = false;
};
