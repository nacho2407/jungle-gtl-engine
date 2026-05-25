#include "ParticleEditorViewer.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleSystemComponent.h"
#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Serialization/ObjectGraphSerializer.h"

#include <algorithm>

namespace
{
	void CollectParticleGraphObjects(UObject* Object, TArray<UObject*>& OutObjects)
	{
		if (!Object || std::find(OutObjects.begin(), OutObjects.end(), Object) != OutObjects.end())
		{
			return;
		}

		OutObjects.push_back(Object);

		if (UParticleSystem* ParticleSystem = Cast<UParticleSystem>(Object))
		{
			for (UParticleEmitter* Emitter : ParticleSystem->Emitters)
			{
				CollectParticleGraphObjects(Emitter, OutObjects);
			}
			return;
		}

		if (UParticleEmitter* Emitter = Cast<UParticleEmitter>(Object))
		{
			for (UParticleLODLevel* LOD : Emitter->LODLevels)
			{
				CollectParticleGraphObjects(LOD, OutObjects);
			}
			return;
		}

		if (UParticleLODLevel* LOD = Cast<UParticleLODLevel>(Object))
		{
			CollectParticleGraphObjects(LOD->RequiredModule, OutObjects);
			CollectParticleGraphObjects(LOD->SpawnModule, OutObjects);
			CollectParticleGraphObjects(LOD->TypeDataModule, OutObjects);
			for (UParticleModule* Module : LOD->Modules)
			{
				CollectParticleGraphObjects(Module, OutObjects);
			}
		}
	}

	void DestroyParticleGraphChildren(UParticleSystem* ParticleSystem)
	{
		TArray<UObject*> Objects;
		CollectParticleGraphObjects(ParticleSystem, Objects);
		for (UObject* Object : Objects)
		{
			if (Object && Object != ParticleSystem && UObjectManager::Get().ContainsObject(Object))
			{
				UObjectManager::Get().DestroyObject(Object);
			}
		}
	}
}

// 파일명 설정 → 기존 선택 및 프리뷰를 초기화 → 새로운 Particle 에셋을 로드하여 시뮬레이션 재시작
bool FParticleEditorViewer::ChangeTarget(const FString& InFileName)
{
	SetFileName(InFileName);
	ClearBaseSelection();
	ClearParticleSelection();
	ClearParticlePreview();

	if (bOwnsParticleSystem)
	{
		UObjectManager::Get().DestroyObject(ParticleSystem);
	}

	ParticleSystem = nullptr;
	bOwnsParticleSystem = false;

	if (!LoadParticleSystemAsset(FPaths::Normalize(InFileName)))
	{
		EnsureDefaultParticleSystem();
	}

	if (!CreatePreviewComponent())
	{
		return false;
	}

	RestartSimulation();
	RefreshSavedSnapshot();
	ClearUndoHistory();
	return true;
}

EEditorTabKind FParticleEditorViewer::GetTabKind() const
{
	return EEditorTabKind::ParticleViewer;
}

const char* FParticleEditorViewer::GetViewerLabel() const
{
	return "Particle System Viewer";
}

// 부모 틱 실행 → 실시간 실행 모드일 경우 프리뷰 컴포넌트에 틱 전송
void FParticleEditorViewer::Tick(float DeltaTime)
{
	FEditorViewer::Tick(DeltaTime);

	if (PreviewComponent && UObjectManager::Get().ContainsObject(PreviewComponent) && IsPlaying() && IsRealtime())
	{
		PreviewComponent->TickComponent(DeltaTime);
	}
}

// 프리뷰 액터 및 선택 상태 해제 → Particle System 메모리 정리
void FParticleEditorViewer::Shutdown()
{
	ClearParticlePreview();

	ClearParticleSelection();
	if (bOwnsParticleSystem)
	{
		UObjectManager::Get().DestroyObject(ParticleSystem);
	}
	ParticleSystem = nullptr;
	bOwnsParticleSystem = false;
	FEditorViewer::Shutdown();
}

bool FParticleEditorViewer::CaptureParticleSnapshot(FString& OutSnapshot) const
{
	OutSnapshot.clear();
	if (!ParticleSystem)
	{
		return false;
	}

	FObjectGraphSerializer Serializer;
	return Serializer.SaveToString(ParticleSystem, "UParticleSystem", OutSnapshot);
}

bool FParticleEditorViewer::RestoreParticleSnapshot(const FString& Snapshot)
{
	if (Snapshot.empty() || !ParticleSystem || bRestoringParticleSnapshot)
	{
		return false;
	}

	FObjectGraphSerializer Serializer;
	UParticleSystem* SnapshotParticleSystem = Cast<UParticleSystem>(Serializer.LoadFromString(Snapshot, "UParticleSystem"));
	if (!SnapshotParticleSystem)
	{
		return false;
	}

	bRestoringParticleSnapshot = true;

	const FString AssetPath = FPaths::Normalize(GetFileName().empty() ? ParticleSystem->GetAssetPath() : GetFileName());
	DestroyParticleGraphChildren(ParticleSystem);
	ParticleSystem->CopyPropertiesFrom(SnapshotParticleSystem);
	SnapshotParticleSystem->Emitters.clear();
	ParticleSystem->SetAssetPath(AssetPath);
	UObjectManager::Get().DestroyObject(SnapshotParticleSystem);
	CacheAllEmitters();
	ClearParticleSelection();
	if (ParticleSystem && !ParticleSystem->Emitters.empty())
	{
		SelectEmitter(0);
	}
	RestartSimulation();

	bRestoringParticleSnapshot = false;
	return true;
}

void FParticleEditorViewer::RefreshSavedSnapshot()
{
	CaptureParticleSnapshot(SavedSnapshot);
	bDirty = false;
}

void FParticleEditorViewer::ClearUndoHistory()
{
	UndoSnapshots.clear();
	RedoSnapshots.clear();
}

void FParticleEditorViewer::CaptureUndoSnapshot(const char* Reason)
{
	(void)Reason;
	if (bRestoringParticleSnapshot)
	{
		return;
	}

	FString Snapshot;
	if (!CaptureParticleSnapshot(Snapshot) || Snapshot.empty())
	{
		return;
	}

	if (!UndoSnapshots.empty() && UndoSnapshots.back() == Snapshot)
	{
		return;
	}

	UndoSnapshots.push_back(std::move(Snapshot));
	if (static_cast<int32>(UndoSnapshots.size()) > MaxParticleUndoSnapshots)
	{
		UndoSnapshots.erase(UndoSnapshots.begin());
	}
	RedoSnapshots.clear();
}

bool FParticleEditorViewer::Undo()
{
	if (UndoSnapshots.empty())
	{
		return false;
	}

	FString CurrentSnapshot;
	CaptureParticleSnapshot(CurrentSnapshot);
	if (!CurrentSnapshot.empty())
	{
		RedoSnapshots.push_back(std::move(CurrentSnapshot));
		if (static_cast<int32>(RedoSnapshots.size()) > MaxParticleUndoSnapshots)
		{
			RedoSnapshots.erase(RedoSnapshots.begin());
		}
	}

	FString Snapshot = std::move(UndoSnapshots.back());
	UndoSnapshots.pop_back();
	if (!RestoreParticleSnapshot(Snapshot))
	{
		return false;
	}

	bDirty = SavedSnapshot.empty() || Snapshot != SavedSnapshot;
	return true;
}

bool FParticleEditorViewer::Redo()
{
	if (RedoSnapshots.empty())
	{
		return false;
	}

	FString CurrentSnapshot;
	CaptureParticleSnapshot(CurrentSnapshot);
	if (!CurrentSnapshot.empty())
	{
		UndoSnapshots.push_back(std::move(CurrentSnapshot));
		if (static_cast<int32>(UndoSnapshots.size()) > MaxParticleUndoSnapshots)
		{
			UndoSnapshots.erase(UndoSnapshots.begin());
		}
	}

	FString Snapshot = std::move(RedoSnapshots.back());
	RedoSnapshots.pop_back();
	if (!RestoreParticleSnapshot(Snapshot))
	{
		return false;
	}

	bDirty = SavedSnapshot.empty() || Snapshot != SavedSnapshot;
	return true;
}

void FParticleEditorViewer::DiscardUnsavedChanges()
{
	if (!SavedSnapshot.empty())
	{
		RestoreParticleSnapshot(SavedSnapshot);
	}
	ClearUndoHistory();
	bDirty = false;
}

void FParticleEditorViewer::CacheAllEmitters()
{
	if (!ParticleSystem)
	{
		return;
	}

	for (UParticleEmitter* Emitter : ParticleSystem->Emitters)
	{
		if (Emitter)
		{
			Emitter->CacheEmitterModuleInfo();
		}
	}
}

// 프리뷰 컴포넌트의 템플릿을 현재 Particle System으로 갱신 → 시뮬레이션 재시작
void FParticleEditorViewer::RestartSimulation()
{
	if (!PreviewComponent || !UObjectManager::Get().ContainsObject(PreviewComponent) || !ParticleSystem)
	{
		PreviewComponent = nullptr;
		return;
	}

	PreviewComponent->SetTemplate(ParticleSystem);
	PreviewComponent->ResetParticles();
}

// Particle 시뮬레이션 재시작 → 레벨 재생 효과를 구현 (RestartSimulation과 동일)
void FParticleEditorViewer::RestartLevel()
{
	RestartSimulation();
}

// 현재 뷰어의 선택 대상을 Particle System 최상단으로 설정 → 하위(Emitter, LOD 등) 선택 상태 초기화
void FParticleEditorViewer::SelectParticleSystem()
{
	if (!ParticleSystem)
	{
		ClearParticleSelection();
		return;
	}

	SelectionType = EParticleEditorSelectionType::ParticleSystem;
	SelectedEmitterIndex = -1;
	SelectedLODIndex = -1;
	SelectedModuleIndex = -1;
}

void FParticleEditorViewer::SelectEmitter(int32 EmitterIndex)
{
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		ClearParticleSelection();
		return;
	}

	SelectionType = EParticleEditorSelectionType::Emitter;
	SelectedEmitterIndex = EmitterIndex;
	SelectedLODIndex = ParticleSystem->Emitters[EmitterIndex] && !ParticleSystem->Emitters[EmitterIndex]->LODLevels.empty() ? 0 : -1;
	SelectedModuleIndex = -1;
}

void FParticleEditorViewer::SelectLOD(int32 LODIndex)
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		SelectedLODIndex = -1;
		SelectedModuleIndex = -1;
		return;
	}

	SelectionType = EParticleEditorSelectionType::LODLevel;
	SelectedLODIndex = LODIndex;
	SelectedModuleIndex = -1;
}

void FParticleEditorViewer::SelectModule(int32 ModuleIndex)
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD || ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(LOD->Modules.size()))
	{
		SelectedModuleIndex = -1;
		return;
	}

	SelectionType = EParticleEditorSelectionType::Module;
	SelectedModuleIndex = ModuleIndex;
}

void FParticleEditorViewer::SelectRequiredModule()
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD || !LOD->RequiredModule)
	{
		SelectionType = LOD ? EParticleEditorSelectionType::LODLevel : EParticleEditorSelectionType::None;
		SelectedModuleIndex = -1;
		return;
	}

	SelectionType = EParticleEditorSelectionType::RequiredModule;
	SelectedModuleIndex = -1;
}

void FParticleEditorViewer::SelectSpawnModule()
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD || !LOD->SpawnModule)
	{
		SelectionType = LOD ? EParticleEditorSelectionType::LODLevel : EParticleEditorSelectionType::None;
		SelectedModuleIndex = -1;
		return;
	}

	SelectionType = EParticleEditorSelectionType::SpawnModule;
	SelectedModuleIndex = -1;
}

void FParticleEditorViewer::SelectTypeDataModule()
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD || !LOD->TypeDataModule)
	{
		SelectionType = LOD ? EParticleEditorSelectionType::LODLevel : EParticleEditorSelectionType::None;
		SelectedModuleIndex = -1;
		return;
	}

	SelectionType = EParticleEditorSelectionType::TypeDataModule;
	SelectedModuleIndex = -1;
}

void FParticleEditorViewer::SelectEmitterModule(int32 EmitterIndex, int32 LODIndex, int32 ModuleIndex)
{
	if (!ParticleSystem || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		ClearParticleSelection();
		return;
	}

	UParticleEmitter* Emitter = ParticleSystem->Emitters[EmitterIndex];
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		ClearParticleSelection();
		return;
	}

	UParticleLODLevel* LOD = Emitter->LODLevels[LODIndex];
	if (!LOD || ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(LOD->Modules.size()))
	{
		ClearParticleSelection();
		return;
	}

	SelectionType = EParticleEditorSelectionType::Module;
	SelectedEmitterIndex = EmitterIndex;
	SelectedLODIndex = LODIndex;
	SelectedModuleIndex = ModuleIndex;
}

UParticleEmitter* FParticleEditorViewer::GetSelectedEmitter() const
{
	if (!ParticleSystem)
	{
		return nullptr;
	}

	if (SelectedEmitterIndex < 0 || SelectedEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return nullptr;
	}

	return ParticleSystem->Emitters[SelectedEmitterIndex];
}

UParticleLODLevel* FParticleEditorViewer::GetSelectedLODLevel() const
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter)
	{
		return nullptr;
	}

	if (SelectedLODIndex < 0 || SelectedLODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return nullptr;
	}

	return Emitter->LODLevels[SelectedLODIndex];
}

UParticleModule* FParticleEditorViewer::GetSelectedModule() const
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD)
	{
		return nullptr;
	}

	if (SelectedModuleIndex < 0 || SelectedModuleIndex >= static_cast<int32>(LOD->Modules.size()))
	{
		return nullptr;
	}

	return LOD->Modules[SelectedModuleIndex];
}

UParticleModuleRequired* FParticleEditorViewer::GetSelectedRequiredModule() const
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	return LOD ? LOD->RequiredModule : nullptr;
}

UParticleModuleSpawn* FParticleEditorViewer::GetSelectedSpawnModule() const
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	return LOD ? LOD->SpawnModule : nullptr;
}

UParticleModuleTypeDataBase* FParticleEditorViewer::GetSelectedTypeDataModule() const
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	return LOD ? LOD->TypeDataModule : nullptr;
}

// 현재 Enum 선택 상태(SelectionType)에 맞춰 활성화된 UObject 기반 인스턴스의 포인터를 동적으로 반환합니다.
UObject* FParticleEditorViewer::GetSelectedObject() const
{
	switch (SelectionType)
	{
	case EParticleEditorSelectionType::ParticleSystem:
		return ParticleSystem;

	case EParticleEditorSelectionType::Emitter:
		return GetSelectedEmitter();

	case EParticleEditorSelectionType::LODLevel:
		return GetSelectedLODLevel();

	case EParticleEditorSelectionType::RequiredModule:
	{
		UParticleLODLevel* LOD = GetSelectedLODLevel();
		return LOD ? LOD->RequiredModule : nullptr;
	}

	case EParticleEditorSelectionType::SpawnModule:
	{
		UParticleLODLevel* LOD = GetSelectedLODLevel();
		return LOD ? LOD->SpawnModule : nullptr;
	}

	case EParticleEditorSelectionType::TypeDataModule:
	{
		UParticleLODLevel* LOD = GetSelectedLODLevel();
		return LOD ? LOD->TypeDataModule : nullptr;
	}

	case EParticleEditorSelectionType::Module:
		return GetSelectedModule();

	case EParticleEditorSelectionType::None:
	default:
		return nullptr;
	}
}

// Viewer의 실시간 시뮬레이션(Realtime) 렌더링 모드 활성화 여부를 설정합니다.
void FParticleEditorViewer::SetRealtime(bool bInRealtime)
{
	Client.SetRealtime(bInRealtime);
}

// Viewer의 렌더링 뷰 모드(Wireframe, Lit 등)를 설정합니다.
void FParticleEditorViewer::SetViewMode(EViewMode InViewMode)
{
	Client.SetViewMode(InViewMode);
}

// 현재 Viewer에 적용된 렌더링 뷰 모드 값을 반환합니다.
EViewMode FParticleEditorViewer::GetViewMode() const
{
	return Client.GetViewMode();
}

// Particle System에 기본 LOD와 Module이 포함된 새 Emitter를 생성하여 추가하고 시뮬레이션을 재시작합니다.
void FParticleEditorViewer::AddEmitter()
{
	if (!ParticleSystem)
	{
		return;
	}

	CaptureUndoSnapshot("AddEmitter");

	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	UParticleLODLevel* LOD = CreateDefaultLODLevel(0);
	if (!Emitter || !LOD)
	{
		UObjectManager::Get().DestroyObject(LOD);
		UObjectManager::Get().DestroyObject(Emitter);
		return;
	}
	Emitter->LODLevels.push_back(LOD);
	Emitter->CacheEmitterModuleInfo();

	ParticleSystem->Emitters.push_back(Emitter);

	SelectionType = EParticleEditorSelectionType::Emitter;
	SelectedEmitterIndex = static_cast<int32>(ParticleSystem->Emitters.size()) - 1;
	SelectedLODIndex = 0;
	SelectedModuleIndex = -1;

	MarkDirty();
	RestartSimulation();
}

// 현재 선택된 Emitter를 Particle System에서 제거한 뒤 메모리를 해제하고 선택 상태를 갱신합니다.
void FParticleEditorViewer::DeleteSelectedEmitter()
{
	if (!ParticleSystem)
	{
		return;
	}

	if (SelectedEmitterIndex < 0 || SelectedEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	CaptureUndoSnapshot("DeleteEmitter");

	UParticleEmitter* Emitter = ParticleSystem->Emitters[SelectedEmitterIndex];
	ParticleSystem->Emitters.erase(ParticleSystem->Emitters.begin() + SelectedEmitterIndex);
	UObjectManager::Get().DestroyObject(Emitter);

	if (ParticleSystem->Emitters.empty())
	{
		ClearParticleSelection();
	}
	else
	{
		SelectedEmitterIndex = std::clamp(SelectedEmitterIndex, 0, static_cast<int32>(ParticleSystem->Emitters.size()) - 1);
		SelectedLODIndex = 0;
		SelectedModuleIndex = -1;
		SelectionType = EParticleEditorSelectionType::Emitter;
	}

	MarkDirty();
	RestartSimulation();
}

void FParticleEditorViewer::DeleteSelection()
{
	switch (SelectionType)
	{
	case EParticleEditorSelectionType::Module:
		DeleteSelectedModule();
		break;
	case EParticleEditorSelectionType::Emitter:
	case EParticleEditorSelectionType::LODLevel:
		DeleteSelectedEmitter();
		break;
	default:
		break;
	}
}

// Particle System 내에서 Emitter의 순서를 변경(이동)하고 변경된 인덱스에 맞게 선택 상태를 갱신합니다.
void FParticleEditorViewer::MoveEmitter(int32 FromIndex, int32 ToIndex)
{
	if (!ParticleSystem)
	{
		return;
	}

	const int32 EmitterCount = static_cast<int32>(ParticleSystem->Emitters.size());
	if (FromIndex < 0 || FromIndex >= EmitterCount || ToIndex < 0 || ToIndex >= EmitterCount || FromIndex == ToIndex)
	{
		return;
	}

	CaptureUndoSnapshot("MoveEmitter");

	UParticleEmitter* Emitter = ParticleSystem->Emitters[FromIndex];
	ParticleSystem->Emitters.erase(ParticleSystem->Emitters.begin() + FromIndex);
	ParticleSystem->Emitters.insert(ParticleSystem->Emitters.begin() + ToIndex, Emitter);

	SelectedEmitterIndex = ToIndex;
	SelectedLODIndex = GetSelectedEmitter() && !GetSelectedEmitter()->LODLevels.empty() ? std::clamp(SelectedLODIndex, 0, static_cast<int32>(GetSelectedEmitter()->LODLevels.size()) - 1) : -1;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::Emitter;

	MarkDirty();
	RestartSimulation();
}

// 전달받은 Module 클래스 타입에 맞춰 Module을 생성한 뒤, 현재 선택된 LOD의 알맞은 슬롯(Required, Spawn 등) 또는 배열에 추가합니다.
void FParticleEditorViewer::AddModule(UClass* ModuleClass)
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD || !ModuleClass)
	{
		return;
	}

	if (!ModuleClass->IsChildOf(UParticleModule::StaticClass()) || ModuleClass->HasAnyClassFlags(CF_Abstract))
	{
		return;
	}

	CaptureUndoSnapshot("AddModule");

	UParticleModule* Module = Cast<UParticleModule>(NewObject(ModuleClass));
	if (!Module)
	{
		return;
	}

	if (UParticleModuleRequired* RequiredModule = Cast<UParticleModuleRequired>(Module))
	{
		UObjectManager::Get().DestroyObject(LOD->RequiredModule);
		SelectionType = EParticleEditorSelectionType::RequiredModule;
		LOD->RequiredModule = RequiredModule;
		SelectedModuleIndex = -1;
	}
	else if (UParticleModuleSpawn* SpawnModule = Cast<UParticleModuleSpawn>(Module))
	{
		// Spawn rate provider는 LOD의 특수 Module이므로 일반 Modules 배열에 넣지 않음!
		UObjectManager::Get().DestroyObject(LOD->SpawnModule);
		SelectionType = EParticleEditorSelectionType::SpawnModule;
		LOD->SpawnModule = SpawnModule;
		SelectedModuleIndex = -1;
	}
	else if (UParticleModuleTypeDataBase* TypeDataModule = Cast<UParticleModuleTypeDataBase>(Module))
	{
		UObjectManager::Get().DestroyObject(LOD->TypeDataModule);
		SelectionType = EParticleEditorSelectionType::TypeDataModule;
		LOD->TypeDataModule = TypeDataModule;
		SelectedModuleIndex = -1;
	}
	else
	{
		SelectionType = EParticleEditorSelectionType::Module;
		LOD->Modules.push_back(Module);
		SelectedModuleIndex = static_cast<int32>(LOD->Modules.size()) - 1;
	}

	if (UParticleEmitter* Emitter = GetSelectedEmitter())
	{
		Emitter->CacheEmitterModuleInfo();
	}

	MarkDirty();
	RestartSimulation();
}

// 현재 선택된 LOD 내에서 일반 Particle Module들의 순서를 변경(이동)합니다.
void FParticleEditorViewer::MoveModule(int32 FromIndex, int32 ToIndex)
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD)
	{
		return;
	}

	const int32 ModuleCount = static_cast<int32>(LOD->Modules.size());
	if (FromIndex < 0 || FromIndex >= ModuleCount || ToIndex < 0 || ToIndex >= ModuleCount || FromIndex == ToIndex)
	{
		return;
	}

	CaptureUndoSnapshot("MoveModule");

	UParticleModule* Module = LOD->Modules[FromIndex];
	LOD->Modules.erase(LOD->Modules.begin() + FromIndex);
	LOD->Modules.insert(LOD->Modules.begin() + ToIndex, Module);

	SelectedModuleIndex = ToIndex;
	SelectionType = EParticleEditorSelectionType::Module;

	if (UParticleEmitter* Emitter = GetSelectedEmitter())
	{
		Emitter->CacheEmitterModuleInfo();
	}

	MarkDirty();
	RestartSimulation();
}

// 특정 Module을 현재 Emitter에서 제거하고 지정된 타겟 Emitter의 LOD 내부로 이동시킵니다.
void FParticleEditorViewer::MoveModuleToEmitter(int32 ModuleIndex, int32 TargetEmitterIndex)
{
	UParticleLODLevel* SourceLOD = GetSelectedLODLevel();
	if (!ParticleSystem || !SourceLOD)
	{
		return;
	}

	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(SourceLOD->Modules.size()))
	{
		return;
	}

	if (TargetEmitterIndex < 0 || TargetEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	const int32 SourceEmitterIndex = SelectedEmitterIndex;
	if (SourceEmitterIndex == TargetEmitterIndex)
	{
		return;
	}

	CaptureUndoSnapshot("MoveModuleToEmitter");

	UParticleEmitter* SourceEmitter = GetSelectedEmitter();
	UParticleEmitter* TargetEmitter = ParticleSystem->Emitters[TargetEmitterIndex];
	if (!SourceEmitter || !TargetEmitter)
	{
		return;
	}

	const int32 TargetLODIndex = SelectedLODIndex >= 0 ? SelectedLODIndex : 0;
	while (static_cast<int32>(TargetEmitter->LODLevels.size()) <= TargetLODIndex)
	{
		UParticleLODLevel* NewLOD = CreateDefaultLODLevel(static_cast<int32>(TargetEmitter->LODLevels.size()));
		if (!NewLOD)
		{
			return;
		}
		TargetEmitter->LODLevels.push_back(NewLOD);
	}

	UParticleLODLevel* TargetLOD = TargetEmitter->LODLevels[TargetLODIndex];
	if (!TargetLOD)
	{
		return;
	}

	UParticleModule* Module = SourceLOD->Modules[ModuleIndex];
	SourceLOD->Modules.erase(SourceLOD->Modules.begin() + ModuleIndex);
	TargetLOD->Modules.push_back(Module);

	SourceEmitter->CacheEmitterModuleInfo();
	TargetEmitter->CacheEmitterModuleInfo();

	SelectionType = EParticleEditorSelectionType::Module;
	SelectedEmitterIndex = TargetEmitterIndex;
	SelectedLODIndex = TargetLODIndex;
	SelectedModuleIndex = static_cast<int32>(TargetLOD->Modules.size()) - 1;

	MarkDirty();
	RestartSimulation();
}

// 특정 Module을 복제하여 지정된 타겟 Emitter의 LOD에 새롭게 추가(복사)합니다.
void FParticleEditorViewer::CopyModuleToEmitter(int32 ModuleIndex, int32 TargetEmitterIndex)
{
	UParticleLODLevel* SourceLOD = GetSelectedLODLevel();
	if (!ParticleSystem || !SourceLOD)
	{
		return;
	}

	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(SourceLOD->Modules.size()))
	{
		return;
	}

	if (TargetEmitterIndex < 0 || TargetEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()))
	{
		return;
	}

	CaptureUndoSnapshot("CopyModuleToEmitter");

	UParticleEmitter* TargetEmitter = ParticleSystem->Emitters[TargetEmitterIndex];
	if (!TargetEmitter)
	{
		return;
	}

	const int32 TargetLODIndex = SelectedLODIndex >= 0 ? SelectedLODIndex : 0;
	while (static_cast<int32>(TargetEmitter->LODLevels.size()) <= TargetLODIndex)
	{
		UParticleLODLevel* NewLOD = CreateDefaultLODLevel(static_cast<int32>(TargetEmitter->LODLevels.size()));
		if (!NewLOD)
		{
			return;
		}
		TargetEmitter->LODLevels.push_back(NewLOD);
	}

	UParticleLODLevel* TargetLOD = TargetEmitter->LODLevels[TargetLODIndex];
	UParticleModule* SourceModule = SourceLOD->Modules[ModuleIndex];
	UParticleModule* CopiedModule = SourceModule
										? Cast<UParticleModule>(SourceModule->Duplicate())
										: nullptr;
	if (!TargetLOD || !CopiedModule)
	{
		UObjectManager::Get().DestroyObject(CopiedModule);
		return;
	}

	TargetLOD->Modules.push_back(CopiedModule);
	TargetEmitter->CacheEmitterModuleInfo();

	SelectionType = EParticleEditorSelectionType::Module;
	SelectedEmitterIndex = TargetEmitterIndex;
	SelectedLODIndex = TargetLODIndex;
	SelectedModuleIndex = static_cast<int32>(TargetLOD->Modules.size()) - 1;

	MarkDirty();
	RestartSimulation();
}

// 현재 선택된 Module을 LOD에서 제거 및 메모리 해제 후 선택 인덱스와 시뮬레이션을 갱신합니다.
void FParticleEditorViewer::DeleteSelectedModule()
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD || SelectedModuleIndex < 0 || SelectedModuleIndex >= static_cast<int32>(LOD->Modules.size()))
	{
		return;
	}

	CaptureUndoSnapshot("DeleteModule");

	UParticleModule* Module = LOD->Modules[SelectedModuleIndex];
	LOD->Modules.erase(LOD->Modules.begin() + SelectedModuleIndex);
	UObjectManager::Get().DestroyObject(Module);

	SelectedModuleIndex = LOD->Modules.empty()
							  ? -1
							  : std::clamp(SelectedModuleIndex, 0, static_cast<int32>(LOD->Modules.size()) - 1);
	SelectionType = SelectedModuleIndex >= 0
						? EParticleEditorSelectionType::Module
						: EParticleEditorSelectionType::LODLevel;

	if (UParticleEmitter* Emitter = GetSelectedEmitter())
	{
		Emitter->CacheEmitterModuleInfo();
	}

	MarkDirty();
	RestartSimulation();
}

// 현재 편집 중인 Particle System의 변경 사항을 로드했던 파일 경로에 직렬화하여 덮어쓰기로 저장합니다.
bool FParticleEditorViewer::Save()
{
	if (!ParticleSystem)
	{
		return false;
	}

	const FString Path = FPaths::Normalize(GetFileName());
	if (Path.empty())
	{
		return false;
	}

	if (!FResourceManager::Get().SaveParticleSystem(Path, ParticleSystem))
	{
		return false;
	}

	ParticleSystem->SetAssetPath(Path);

	bOwnsParticleSystem = false;
	RefreshSavedSnapshot();

	return true;
}

// 새로운 파일 경로를 인자로 받아 현재 Particle System 에셋을 직렬화하여 저장합니다.
bool FParticleEditorViewer::SaveAs(const FString& InFileName)
{
	if (InFileName.empty())
	{
		return false;
	}

	const FString OldFileName = GetFileName();
	SetFileName(FPaths::Normalize(InFileName));
	const bool bSaved = Save();
	if (!bSaved)
	{
		SetFileName(OldFileName);
		return false;
	}

	if (UEditorEngine* EditorEngine = GetEditorEngine())
	{
		EditorEngine->GetMainPanel().RefreshViewerTabAfterFileNameChange(this, OldFileName);
	}
	return true;
}

// 콘텐츠 브라우저에서 현재 편집 중인 Particle System 에셋의 위치를 찾아 강조 표시합니다.
void FParticleEditorViewer::FindInContentBrowser()
{
	FString TargetPath = FPaths::Normalize(GetFileName());
	if (TargetPath.empty() && ParticleSystem)
	{
		TargetPath = FPaths::Normalize(ParticleSystem->GetAssetPath());
	}
	if (TargetPath.empty())
	{
		return;
	}

	UEditorEngine* EditorEngine = GetEditorEngine();
	if (!EditorEngine)
	{
		return;
	}

	EditorEngine->GetMainPanel().RevealContentBrowserAsset(TargetPath);
}

// 현재 선택된 Emitter에 새로운 LOD 레벨을 생성하여 추가하고 선택 상태를 갱신합니다.
void FParticleEditorViewer::AddLOD()
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter)
	{
		return;
	}

	UParticleLODLevel* LOD = CreateDefaultLODLevel(static_cast<int32>(Emitter->LODLevels.size()));
	if (!LOD)
	{
		return;
	}

	CaptureUndoSnapshot("AddLOD");

	Emitter->LODLevels.push_back(LOD);
	Emitter->CacheEmitterModuleInfo();

	SelectionType = EParticleEditorSelectionType::LODLevel;
	SelectedLODIndex = static_cast<int32>(Emitter->LODLevels.size()) - 1;
	SelectedModuleIndex = -1;

	MarkDirty();
	RestartSimulation();
}

// 현재 선택된 Emitter에서 지정한 인덱스의 LOD 레벨을 제거하고 메모리를 해제합니다 (최소 1개 이상은 유지).
void FParticleEditorViewer::RemoveLOD(int32 LODIndex)
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return;
	}

	if (Emitter->LODLevels.size() <= 1)
	{
		return;
	}

	CaptureUndoSnapshot("RemoveLOD");

	UParticleLODLevel* RemovedLOD = Emitter->LODLevels[LODIndex];
	Emitter->LODLevels.erase(Emitter->LODLevels.begin() + LODIndex);
	UObjectManager::Get().DestroyObject(RemovedLOD);
	Emitter->CacheEmitterModuleInfo();

	SelectedLODIndex = std::clamp(SelectedLODIndex, 0, static_cast<int32>(Emitter->LODLevels.size()) - 1);
	SelectedModuleIndex = -1;

	MarkDirty();
	RestartSimulation();
}

// 현재 Emitter에서 디테일이 가장 높은 최상단(인덱스 0) LOD 레벨을 선택합니다.
void FParticleEditorViewer::SetHighestLOD()
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter || Emitter->LODLevels.empty())
	{
		return;
	}

	SelectedLODIndex = 0;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::LODLevel;
}

// 현재 Emitter에서 디테일이 가장 낮은 최하단(마지막 인덱스) LOD 레벨을 선택합니다.
void FParticleEditorViewer::SetLowestLOD()
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter || Emitter->LODLevels.empty())
	{
		return;
	}

	SelectedLODIndex = static_cast<int32>(Emitter->LODLevels.size()) - 1;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::LODLevel;
}

// 현재 선택된 LOD 레벨보다 한 단계 디테일이 낮은(인덱스 증가) LOD로 선택 대상을 변경합니다.
void FParticleEditorViewer::SelectLowerLOD()
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter || SelectedLODIndex < 0 || SelectedLODIndex + 1 >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return;
	}

	++SelectedLODIndex;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::LODLevel;
}

// 현재 선택된 LOD 레벨보다 한 단계 디테일이 높은(인덱스 감소) LOD로 선택 대상을 변경합니다.
void FParticleEditorViewer::SelectUpperLOD()
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter || SelectedLODIndex <= 0 || SelectedLODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return;
	}

	--SelectedLODIndex;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::LODLevel;
}

// 뷰어 월드에 배치된 프리뷰 액터와 Particle 컴포넌트를 소거하여 렌더링 상태를 완전 초기화합니다.
void FParticleEditorViewer::ClearParticlePreview()
{
	if (PreviewComponent && UObjectManager::Get().ContainsObject(PreviewComponent))
	{
		PreviewComponent->SetTemplate(nullptr);
		PreviewComponent->ResetParticles();
	}

	PreviewComponent = nullptr;

	if (PreviewActor && UObjectManager::Get().ContainsObject(PreviewActor))
	{
		UWorld* World = PreviewWorld && UObjectManager::Get().ContainsObject(PreviewWorld) ? PreviewWorld : PreviewActor->GetFocusedWorld();
		if (World && PreviewActor->GetFocusedWorld() == World)
		{
			World->DestroyActor(PreviewActor);
			World->SyncSpatialIndex();
		}
	}

	PreviewActor = nullptr;
	PreviewWorld = nullptr;
}

// Emitter, LOD, Module에 대한 모든 선택 인덱스와 타입을 초기화하여 아무것도 선택되지 않은 상태로 만듭니다.
void FParticleEditorViewer::ClearParticleSelection()
{
	SelectedEmitterIndex = -1;
	SelectedLODIndex = -1;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::None;
}

// 뷰어 포커스 월드에 임시 액터를 스폰하고, 현재 Particle System을 시각적으로 렌더링할 Particle 컴포넌트를 부착합니다.
bool FParticleEditorViewer::CreatePreviewComponent()
{
	if (PreviewComponent && UObjectManager::Get().ContainsObject(PreviewComponent))
	{
		PreviewComponent->SetTemplate(ParticleSystem);
		return true;
	}
	PreviewComponent = nullptr;

	UWorld* World = GetClient().GetFocusedWorld();
	if (!World)
	{
		return false;
	}

	PreviewActor = World->SpawnActor<AActor>();
	if (!PreviewActor)
	{
		return false;
	}

	PreviewWorld = World;

	PreviewActor->SetFName(FName("ParticleViewerActor"));
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	PreviewComponent = PreviewActor->AddComponent<UParticleSystemComponent>();
	if (!PreviewComponent)
	{
		ClearParticlePreview();
		return false;
	}

	PreviewActor->SetRootComponent(PreviewComponent);
	PreviewComponent->SetTemplate(ParticleSystem);
	World->SyncSpatialIndex();
	return true;
}

// Resource Manager를 통해 지정된 경로의 Particle 에셋을 로드하고, 성공 시 첫 번째 Emitter를 기본 선택 상태로 만듭니다.
bool FParticleEditorViewer::LoadParticleSystemAsset(const FString& InFileName)
{
	ParticleSystem = nullptr;
	bOwnsParticleSystem = false;

	if (InFileName.empty())
	{
		return false;
	}

	ParticleSystem = FResourceManager::Get().LoadParticleSystem(InFileName);
	if (!ParticleSystem)
	{
		return false;
	}

	bOwnsParticleSystem = false;
	bDirty = false;

	if (!ParticleSystem->Emitters.empty())
	{
		SelectionType = EParticleEditorSelectionType::Emitter;
		SelectedEmitterIndex = 0;
		SelectedLODIndex = ParticleSystem->Emitters[0] && !ParticleSystem->Emitters[0]->LODLevels.empty() ? 0 : -1;
		SelectedModuleIndex = -1;
	}
	else
	{
		ClearParticleSelection();
	}

	return true;
}

// 로드된 Particle System이 없을 경우 빈 에셋을 방지하기 위해 뷰어 소유의 기본 Particle System과 초기 Emitter를 강제로 생성합니다.
void FParticleEditorViewer::EnsureDefaultParticleSystem()
{
	if (!ParticleSystem)
	{
		ParticleSystem = NewObject<UParticleSystem>();
		bOwnsParticleSystem = true;
		ParticleSystem->SetAssetPath(FPaths::Normalize(GetFileName()));
	}

	// LOD 거리 설정이 비어있으면 기본값으로 LOD 0 거리 0.0f 추가
	if (ParticleSystem && ParticleSystem->LODDistances.empty())
	{
		ParticleSystem->LODDistances.push_back(0.0f);
	}

	if (ParticleSystem && ParticleSystem->Emitters.empty())
	{
		UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
		UParticleLODLevel* LOD = CreateDefaultLODLevel(0);

		if (Emitter && LOD)
		{
			Emitter->LODLevels.push_back(LOD);
			Emitter->CacheEmitterModuleInfo();
			ParticleSystem->Emitters.push_back(Emitter);

			SelectedEmitterIndex = 0;
			SelectedLODIndex = 0;
			SelectedModuleIndex = -1;
			SelectionType = EParticleEditorSelectionType::Emitter;
		}
	}
}

// 지정된 레벨 인덱스를 기반으로 Particle 렌더링에 필수적인 Module(Required, Spawn, TypeData)이 세팅된 새 LOD 레벨 객체를 생성하여 반환합니다.
UParticleLODLevel* FParticleEditorViewer::CreateDefaultLODLevel(int32 Level)
{
	UParticleLODLevel* LOD = NewObject<UParticleLODLevel>();
	if (!LOD)
	{
		return nullptr;
	}

	LOD->Level = Level;
	LOD->bEnabled = true;
	LOD->RequiredModule = NewObject<UParticleModuleRequired>();
	LOD->SpawnModule = NewObject<UParticleModuleSpawn>();
	LOD->TypeDataModule = NewObject<UParticleModuleTypeDataBase>();
	return LOD;
}
