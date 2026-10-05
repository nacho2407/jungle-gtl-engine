#include "ParticleEditorViewer.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleSystemComponent.h"
#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorMainPanel.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Particle/ParticleModules.h"
#include "Serialization/ObjectGraphSerializer.h"

#include <algorithm>
#include <cmath>

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

	TArray<int32> MakeSortedUniqueIndices(const TArray<int32>& Indices, int32 Count)
	{
		TArray<int32> Result;
		for (int32 Index : Indices)
		{
			if (Index >= 0 && Index < Count && std::find(Result.begin(), Result.end(), Index) == Result.end())
			{
				Result.push_back(Index);
			}
		}
		std::sort(Result.begin(), Result.end());
		return Result;
	}


	float GetLODStartSizeForTrailPreview(const UParticleLODLevel* LOD)
	{
		if (LOD == nullptr)
		{
			return 10.0f;
		}

		for (UParticleModule* Module : LOD->Modules)
		{
			const UParticleModuleSize* SizeModule = Cast<UParticleModuleSize>(Module);
			if (SizeModule != nullptr && SizeModule->bEnabled)
			{
				return std::max(std::fabs(SizeModule->StartSize.Constant.X), 1.0f);
			}
		}

		return 10.0f;
	}

	float GetTrailPreviewRadius(const UParticleLODLevel* LOD)
	{
		const UParticleModuleTypeDataRibbon* RibbonTypeData = LOD != nullptr
			? Cast<UParticleModuleTypeDataRibbon>(LOD->TypeDataModule)
			: nullptr;
		if (RibbonTypeData == nullptr)
		{
			return 100.0f;
		}

		const float TrailSize = RibbonTypeData->bUseParticleSizeAsWidth
			? GetLODStartSizeForTrailPreview(LOD)
			: std::max(RibbonTypeData->RibbonWidth, 1.0f);
		return std::max(TrailSize * 6.0f, 3.0f);
	}

	int32 GetMaxParticleSystemLODCount(const UParticleSystem* ParticleSystem)
	{
		int32 MaxLODCount = 1;
		if (!ParticleSystem)
		{
			return MaxLODCount;
		}
		for (const UParticleEmitter* Emitter : ParticleSystem->Emitters)
		{
			if (Emitter)
			{
				MaxLODCount = std::max(MaxLODCount, static_cast<int32>(Emitter->LODLevels.size()));
			}
		}
		return MaxLODCount;
	}

	bool SyncParticleSystemLODDistancesToLODCount(UParticleSystem* ParticleSystem)
	{
		if (!ParticleSystem)
		{
			return false;
		}
		const int32 DesiredCount = GetMaxParticleSystemLODCount(ParticleSystem);
		bool bChanged = false;
		if (ParticleSystem->LODDistances.empty())
		{
			ParticleSystem->LODDistances.push_back(0.0f);
			bChanged = true;
		}
		while (static_cast<int32>(ParticleSystem->LODDistances.size()) < DesiredCount)
		{
			const float Previous = ParticleSystem->LODDistances.empty() ? 0.0f : ParticleSystem->LODDistances.back();
			ParticleSystem->LODDistances.push_back(Previous + 500.0f);
			bChanged = true;
		}
		while (static_cast<int32>(ParticleSystem->LODDistances.size()) > DesiredCount)
		{
			ParticleSystem->LODDistances.pop_back();
			bChanged = true;
		}
		ParticleSystem->LODDistances[0] = 0.0f;
		return bChanged;
	}

	/**
	 * @brief 단일 particle module을 같은 클래스의 새 객체로 복제합니다.
	 */
	UParticleModule* DuplicateParticleModule(UParticleModule* SourceModule)
	{
		return SourceModule ? Cast<UParticleModule>(SourceModule->Duplicate()) : nullptr;
	}

	/**
	 * @brief 아직 object manager에 남아있는 particle module 객체를 해제합니다.
	 */
	void DestroyParticleModuleIfLive(UParticleModule* Module)
	{
		if (Module && UObjectManager::Get().ContainsObject(Module))
		{
			UObjectManager::Get().DestroyObject(Module);
		}
	}

	/**
	 * @brief module 배열에 들어있는 객체들을 모두 해제하고 배열을 비웁니다.
	 */
	void DestroyParticleModules(TArray<UParticleModule*>& Modules)
	{
		for (UParticleModule* Module : Modules)
		{
			DestroyParticleModuleIfLive(Module);
		}
		Modules.clear();
	}

	/**
	 * @brief 지정된 module 포인터가 module 배열에 포함되는지 확인합니다.
	 */
	bool IsParticleModuleInList(UParticleModule* Module, const TArray<UParticleModule*>& Modules)
	{
		return Module != nullptr && std::find(Modules.begin(), Modules.end(), Module) != Modules.end();
	}

	/**
	 * @brief 정렬된 module index 목록에 대응하는 module 포인터들을 추출합니다.
	 */
	TArray<UParticleModule*> GetModulesBySortedIndices(UParticleLODLevel* LOD, const TArray<int32>& SortedIndices)
	{
		TArray<UParticleModule*> Result;
		if (!LOD)
		{
			return Result;
		}

		for (int32 Index : SortedIndices)
		{
			if (Index >= 0 && Index < static_cast<int32>(LOD->Modules.size()))
			{
				Result.push_back(LOD->Modules[static_cast<size_t>(Index)]);
			}
		}
		return Result;
	}

	/**
	 * @brief 지정된 LOD가 요청된 module slot 목록을 모두 포함하는지 확인합니다.
	 */
	bool HasModuleSlots(UParticleLODLevel* LOD, const TArray<int32>& SortedIndices)
	{
		if (!LOD)
		{
			return false;
		}

		for (int32 Index : SortedIndices)
		{
			if (Index < 0 || Index >= static_cast<int32>(LOD->Modules.size()))
			{
				return false;
			}
		}
		return true;
	}

	/**
	 * @brief module 목록 전체를 같은 순서의 새 module 목록으로 복제합니다.
	 */
	TArray<UParticleModule*> DuplicateModules(const TArray<UParticleModule*>& SourceModules)
	{
		TArray<UParticleModule*> Result;
		Result.reserve(SourceModules.size());
		for (UParticleModule* SourceModule : SourceModules)
		{
			UParticleModule* CopiedModule = DuplicateParticleModule(SourceModule);
			if (!CopiedModule)
			{
				DestroyParticleModules(Result);
				return {};
			}
			Result.push_back(CopiedModule);
		}
		return Result;
	}

	/**
	 * @brief 단일 LOD 안에서 지정된 module slot 묶음을 새 위치로 이동합니다.
	 */
	int32 MoveModuleSlots(UParticleLODLevel* LOD, const TArray<int32>& SortedIndices, int32 ToIndex)
	{
		if (!LOD || SortedIndices.empty())
		{
			return -1;
		}

		// 이동 대상 module 보관
		TArray<UParticleModule*> MovingModules;
		MovingModules.reserve(SortedIndices.size());
		for (int32 Index : SortedIndices)
		{
			if (Index < 0 || Index >= static_cast<int32>(LOD->Modules.size()))
			{
				return -1;
			}
			MovingModules.push_back(LOD->Modules[static_cast<size_t>(Index)]);
		}

		// 기존 slot 제거
		for (auto It = SortedIndices.rbegin(); It != SortedIndices.rend(); ++It)
		{
			LOD->Modules.erase(LOD->Modules.begin() + *It);
		}

		// 제거된 앞쪽 slot 수 반영
		int32 AdjustedToIndex = ToIndex;
		for (int32 Index : SortedIndices)
		{
			if (Index < ToIndex)
			{
				--AdjustedToIndex;
			}
		}

		AdjustedToIndex = std::clamp(AdjustedToIndex, 0, static_cast<int32>(LOD->Modules.size()));
		LOD->Modules.insert(LOD->Modules.begin() + AdjustedToIndex, MovingModules.begin(), MovingModules.end());
		return AdjustedToIndex;
	}

	/**
	 * @brief 단일 LOD 안에서 지정된 module slot 묶음을 제거합니다.
	 */
	void RemoveModuleSlots(
		UParticleLODLevel* LOD,
		const TArray<int32>& SortedIndices,
		const TArray<UParticleModule*>& TransferredModules)
	{
		if (!LOD)
		{
			return;
		}

		// 뒤쪽 slot부터 제거
		for (auto It = SortedIndices.rbegin(); It != SortedIndices.rend(); ++It)
		{
			const int32 Index = *It;
			if (Index < 0 || Index >= static_cast<int32>(LOD->Modules.size()))
			{
				continue;
			}

			UParticleModule* RemovedModule = LOD->Modules[static_cast<size_t>(Index)];
			LOD->Modules.erase(LOD->Modules.begin() + Index);
			if (!IsParticleModuleInList(RemovedModule, TransferredModules))
			{
				DestroyParticleModuleIfLive(RemovedModule);
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
		SimulationTime += std::max(0.0f, DeltaTime);
		UpdateTrailPreviewMotion(DeltaTime);
		PreviewComponent->TickComponent(DeltaTime);
	}
	else
	{
		UpdateTrailPreviewMotion(0.0f);
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
	SimulationTime = 0.0f;
	TrailPreviewTime = 0.0f;

	if (!PreviewComponent || !UObjectManager::Get().ContainsObject(PreviewComponent) || !ParticleSystem)
	{
		PreviewComponent = nullptr;
		return;
	}

	PreviewComponent->SetTemplate(ParticleSystem);
	PreviewComponent->ResetParticles();
	UpdateTrailPreviewMotion(0.0f);
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

bool FParticleEditorViewer::CanEditLODTopology(int32 LODIndex) const
{
	return LODIndex == 0;
}

bool FParticleEditorViewer::CanEditSelectedLODTopology() const
{
	return CanEditLODTopology(SelectedLODIndex);
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

void FParticleEditorViewer::DuplicateEmitter(int32 EmitterIndex)
{
	TArray<int32> EmitterIndices;
	EmitterIndices.push_back(EmitterIndex);
	CopyEmittersToIndex(EmitterIndices, EmitterIndex + 1);
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

void FParticleEditorViewer::MoveEmittersToIndex(const TArray<int32>& EmitterIndices, int32 ToIndex)
{
	if (!ParticleSystem)
	{
		return;
	}

	const int32 EmitterCount = static_cast<int32>(ParticleSystem->Emitters.size());
	TArray<int32> SortedIndices = MakeSortedUniqueIndices(EmitterIndices, EmitterCount);
	if (SortedIndices.empty() || ToIndex < 0 || ToIndex >= EmitterCount)
	{
		return;
	}

	CaptureUndoSnapshot("MoveEmitters");

	TArray<UParticleEmitter*> MovingEmitters;
	for (int32 Index : SortedIndices)
	{
		MovingEmitters.push_back(ParticleSystem->Emitters[Index]);
	}

	for (auto It = SortedIndices.rbegin(); It != SortedIndices.rend(); ++It)
	{
		ParticleSystem->Emitters.erase(ParticleSystem->Emitters.begin() + *It);
	}

	int32 AdjustedToIndex = ToIndex;
	for (int32 Index : SortedIndices)
	{
		if (Index < ToIndex)
		{
			--AdjustedToIndex;
		}
	}
	AdjustedToIndex = std::clamp(AdjustedToIndex, 0, static_cast<int32>(ParticleSystem->Emitters.size()));

	ParticleSystem->Emitters.insert(
		ParticleSystem->Emitters.begin() + AdjustedToIndex,
		MovingEmitters.begin(),
		MovingEmitters.end());

	SelectedEmitterIndex = AdjustedToIndex;
	SelectedLODIndex = 0;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::Emitter;

	MarkDirty();
	RestartSimulation();
}

void FParticleEditorViewer::CopyEmittersToIndex(const TArray<int32>& EmitterIndices, int32 ToIndex)
{
	if (!ParticleSystem)
	{
		return;
	}

	const int32 EmitterCount = static_cast<int32>(ParticleSystem->Emitters.size());
	TArray<int32> SortedIndices = MakeSortedUniqueIndices(EmitterIndices, EmitterCount);
	if (SortedIndices.empty())
	{
		return;
	}

	const int32 InsertIndex = std::clamp(ToIndex, 0, EmitterCount);
	TArray<UParticleEmitter*> CopiedEmitters;
	for (int32 Index : SortedIndices)
	{
		UParticleEmitter* SourceEmitter = ParticleSystem->Emitters[Index];
		UParticleEmitter* CopiedEmitter = SourceEmitter
											  ? Cast<UParticleEmitter>(SourceEmitter->Duplicate())
											  : nullptr;
		if (CopiedEmitter)
		{
			CopiedEmitter->CacheEmitterModuleInfo();
			CopiedEmitters.push_back(CopiedEmitter);
		}
	}

	if (CopiedEmitters.empty())
	{
		return;
	}

	CaptureUndoSnapshot("CopyEmitters");

	ParticleSystem->Emitters.insert(
		ParticleSystem->Emitters.begin() + InsertIndex,
		CopiedEmitters.begin(),
		CopiedEmitters.end());

	SelectedEmitterIndex = InsertIndex;
	SelectedLODIndex = 0;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::Emitter;

	MarkDirty();
	RestartSimulation();
}

// 전달받은 Module 클래스 타입에 맞춰 Module을 생성한 뒤, 현재 선택된 LOD의 알맞은 슬롯(Required, Spawn 등) 또는 배열에 추가합니다.
void FParticleEditorViewer::AddModule(UClass* ModuleClass)
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	UParticleLODLevel* LOD0 = GetSelectedLODLevel();
	if (!Emitter || !LOD0 || !ModuleClass)
	{
		return;
	}

	if (!CanEditSelectedLODTopology())
	{
		return;
	}

	if (!ModuleClass->IsChildOf(UParticleModule::StaticClass()) || ModuleClass->HasAnyClassFlags(CF_Abstract))
	{
		return;
	}

	UParticleModule* Module = Cast<UParticleModule>(NewObject(ModuleClass));
	if (!Module)
	{
		return;
	}

	// lower LOD 동기화용 복제 module
	TArray<UParticleModule*> LowerLODModules;
	LowerLODModules.reserve(Emitter->LODLevels.size() > 0 ? Emitter->LODLevels.size() - 1 : 0);
	for (int32 LODIndex = 1; LODIndex < static_cast<int32>(Emitter->LODLevels.size()); ++LODIndex)
	{
		if (!Emitter->LODLevels[static_cast<size_t>(LODIndex)])
		{
			DestroyParticleModuleIfLive(Module);
			DestroyParticleModules(LowerLODModules);
			return;
		}

		UParticleModule* CopiedModule = DuplicateParticleModule(Module);
		if (!CopiedModule)
		{
			DestroyParticleModuleIfLive(Module);
			DestroyParticleModules(LowerLODModules);
			return;
		}
		LowerLODModules.push_back(CopiedModule);
	}

	CaptureUndoSnapshot("AddModule");

	if (UParticleModuleRequired* RequiredModule = Cast<UParticleModuleRequired>(Module))
	{
		// RequiredModule slot class 동기화
		DestroyParticleModuleIfLive(LOD0->RequiredModule);
		SelectionType = EParticleEditorSelectionType::RequiredModule;
		LOD0->RequiredModule = RequiredModule;
		SelectedModuleIndex = -1;
		for (int32 LODIndex = 1; LODIndex < static_cast<int32>(Emitter->LODLevels.size()); ++LODIndex)
		{
			UParticleLODLevel* LOD = Emitter->LODLevels[static_cast<size_t>(LODIndex)];
			DestroyParticleModuleIfLive(LOD->RequiredModule);
			LOD->RequiredModule = Cast<UParticleModuleRequired>(LowerLODModules[static_cast<size_t>(LODIndex - 1)]);
		}
	}
	else if (UParticleModuleSpawn* SpawnModule = Cast<UParticleModuleSpawn>(Module))
	{
		// SpawnModule slot class 동기화
		DestroyParticleModuleIfLive(LOD0->SpawnModule);
		SelectionType = EParticleEditorSelectionType::SpawnModule;
		LOD0->SpawnModule = SpawnModule;
		SelectedModuleIndex = -1;
		for (int32 LODIndex = 1; LODIndex < static_cast<int32>(Emitter->LODLevels.size()); ++LODIndex)
		{
			UParticleLODLevel* LOD = Emitter->LODLevels[static_cast<size_t>(LODIndex)];
			DestroyParticleModuleIfLive(LOD->SpawnModule);
			LOD->SpawnModule = Cast<UParticleModuleSpawn>(LowerLODModules[static_cast<size_t>(LODIndex - 1)]);
		}
	}
	else if (UParticleModuleTypeDataBase* TypeDataModule = Cast<UParticleModuleTypeDataBase>(Module))
	{
		// TypeDataModule slot class 동기화
		DestroyParticleModuleIfLive(LOD0->TypeDataModule);
		SelectionType = EParticleEditorSelectionType::TypeDataModule;
		LOD0->TypeDataModule = TypeDataModule;
		SelectedModuleIndex = -1;
		for (int32 LODIndex = 1; LODIndex < static_cast<int32>(Emitter->LODLevels.size()); ++LODIndex)
		{
			UParticleLODLevel* LOD = Emitter->LODLevels[static_cast<size_t>(LODIndex)];
			DestroyParticleModuleIfLive(LOD->TypeDataModule);
			LOD->TypeDataModule = Cast<UParticleModuleTypeDataBase>(LowerLODModules[static_cast<size_t>(LODIndex - 1)]);
		}
	}
	else
	{
		// 일반 module slot 추가
		SelectionType = EParticleEditorSelectionType::Module;
		LOD0->Modules.push_back(Module);
		SelectedModuleIndex = static_cast<int32>(LOD0->Modules.size()) - 1;
		for (int32 LODIndex = 1; LODIndex < static_cast<int32>(Emitter->LODLevels.size()); ++LODIndex)
		{
			UParticleLODLevel* LOD = Emitter->LODLevels[static_cast<size_t>(LODIndex)];
			LOD->Modules.push_back(LowerLODModules[static_cast<size_t>(LODIndex - 1)]);
		}
	}

	RefreshEmitterAfterTopologyEdit(Emitter);
}

// 현재 선택된 LOD 내에서 일반 Particle Module들의 순서를 변경(이동)합니다.
void FParticleEditorViewer::MoveModule(int32 FromIndex, int32 ToIndex)
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	UParticleLODLevel* LOD0 = GetSelectedLODLevel();
	if (!Emitter || !LOD0 || !CanEditSelectedLODTopology())
	{
		return;
	}

	const int32 ModuleCount = static_cast<int32>(LOD0->Modules.size());
	if (FromIndex < 0 || FromIndex >= ModuleCount || ToIndex < 0 || ToIndex >= ModuleCount || FromIndex == ToIndex)
	{
		return;
	}

	TArray<int32> ModuleIndices;
	ModuleIndices.push_back(FromIndex);
	const TArray<int32> SortedIndices = MakeSortedUniqueIndices(ModuleIndices, ModuleCount);
	for (UParticleLODLevel* LOD : Emitter->LODLevels)
	{
		if (!HasModuleSlots(LOD, SortedIndices))
		{
			return;
		}
	}

	CaptureUndoSnapshot("MoveModule");

	// 모든 LOD의 같은 module slot 이동
	int32 AdjustedToIndex = -1;
	for (UParticleLODLevel* LOD : Emitter->LODLevels)
	{
		const int32 MovedIndex = MoveModuleSlots(LOD, SortedIndices, ToIndex);
		if (LOD == LOD0)
		{
			AdjustedToIndex = MovedIndex;
		}
	}

	SelectedModuleIndex = AdjustedToIndex >= 0 ? AdjustedToIndex : ToIndex;
	SelectionType = EParticleEditorSelectionType::Module;

	RefreshEmitterAfterTopologyEdit(Emitter);
}

void FParticleEditorViewer::MoveModules(int32 SourceEmitterIndex, int32 SourceLODIndex, const TArray<int32>& ModuleIndices, int32 ToIndex)
{
	if (!ParticleSystem ||
		SourceEmitterIndex < 0 ||
		SourceEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()) ||
		!CanEditLODTopology(SourceLODIndex))
	{
		return;
	}

	UParticleEmitter* SourceEmitter = ParticleSystem->Emitters[SourceEmitterIndex];
	if (!SourceEmitter || SourceLODIndex < 0 || SourceLODIndex >= static_cast<int32>(SourceEmitter->LODLevels.size()))
	{
		return;
	}

	UParticleLODLevel* SourceLOD = SourceEmitter->LODLevels[SourceLODIndex];
	if (!SourceLOD)
	{
		return;
	}

	const int32 ModuleCount = static_cast<int32>(SourceLOD->Modules.size());
	TArray<int32> SortedIndices = MakeSortedUniqueIndices(ModuleIndices, ModuleCount);
	if (SortedIndices.empty() || ToIndex < 0 || ToIndex >= ModuleCount)
	{
		return;
	}
	for (UParticleLODLevel* LOD : SourceEmitter->LODLevels)
	{
		if (!HasModuleSlots(LOD, SortedIndices))
		{
			return;
		}
	}

	CaptureUndoSnapshot("MoveModules");

	// 모든 LOD의 같은 module slot 묶음 이동
	int32 AdjustedToIndex = -1;
	for (UParticleLODLevel* LOD : SourceEmitter->LODLevels)
	{
		const int32 MovedIndex = MoveModuleSlots(LOD, SortedIndices, ToIndex);
		if (LOD == SourceLOD)
		{
			AdjustedToIndex = MovedIndex;
		}
	}

	SelectedEmitterIndex = SourceEmitterIndex;
	SelectedLODIndex = SourceLODIndex;
	SelectedModuleIndex = AdjustedToIndex >= 0 ? AdjustedToIndex : ToIndex;
	SelectionType = EParticleEditorSelectionType::Module;

	RefreshEmitterAfterTopologyEdit(SourceEmitter);
}

// 특정 Module을 현재 Emitter에서 제거하고 지정된 타겟 Emitter의 LOD 내부로 이동시킵니다.
void FParticleEditorViewer::MoveModuleToEmitter(int32 ModuleIndex, int32 TargetEmitterIndex)
{
	TArray<int32> ModuleIndices;
	ModuleIndices.push_back(ModuleIndex);
	MoveModulesToEmitter(SelectedEmitterIndex, SelectedLODIndex, ModuleIndices, TargetEmitterIndex);
}

void FParticleEditorViewer::MoveModulesToEmitter(int32 SourceEmitterIndex, int32 SourceLODIndex, const TArray<int32>& ModuleIndices, int32 TargetEmitterIndex)
{
	if (!ParticleSystem ||
		SourceEmitterIndex < 0 || SourceEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()) ||
		TargetEmitterIndex < 0 || TargetEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()) ||
		!CanEditLODTopology(SourceLODIndex))
	{
		return;
	}

	const int32 SourceEmitterCount = static_cast<int32>(ParticleSystem->Emitters.size());
	if (SourceEmitterIndex >= SourceEmitterCount || TargetEmitterIndex >= SourceEmitterCount)
	{
		return;
	}

	if (SourceEmitterIndex == TargetEmitterIndex)
	{
		return;
	}

	UParticleEmitter* SourceEmitter = ParticleSystem->Emitters[SourceEmitterIndex];
	UParticleEmitter* TargetEmitter = ParticleSystem->Emitters[TargetEmitterIndex];
	if (!SourceEmitter || !TargetEmitter || SourceEmitter->LODLevels.empty())
	{
		return;
	}

	if (TargetEmitter->LODLevels.empty())
	{
		UParticleLODLevel* TargetLOD0 = CreateDefaultLODLevel(0);
		if (!TargetLOD0)
		{
			return;
		}
		TargetEmitter->LODLevels.push_back(TargetLOD0);
	}

	UParticleLODLevel* SourceLOD0 = SourceEmitter->LODLevels[0];
	if (!SourceLOD0)
	{
		return;
	}

	const int32 ModuleCount = static_cast<int32>(SourceLOD0->Modules.size());
	const TArray<int32> SortedIndices = MakeSortedUniqueIndices(ModuleIndices, ModuleCount);
	if (SortedIndices.empty())
	{
		return;
	}
	for (UParticleLODLevel* SourceLOD : SourceEmitter->LODLevels)
	{
		if (!HasModuleSlots(SourceLOD, SortedIndices))
		{
			return;
		}
	}

	// target LOD별 이동 대상 준비
	const TArray<UParticleModule*> SourceLOD0Modules = GetModulesBySortedIndices(SourceLOD0, SortedIndices);
	TArray<TArray<UParticleModule*>> TargetModulesByLOD;
	TargetModulesByLOD.resize(TargetEmitter->LODLevels.size());
	TArray<UParticleModule*> TransferredModules;
	TArray<UParticleModule*> OwnedDuplicatedModules;
	for (int32 TargetLODIndex = 0; TargetLODIndex < static_cast<int32>(TargetEmitter->LODLevels.size()); ++TargetLODIndex)
	{
		UParticleLODLevel* SourceLOD = TargetLODIndex < static_cast<int32>(SourceEmitter->LODLevels.size())
			? SourceEmitter->LODLevels[static_cast<size_t>(TargetLODIndex)]
			: nullptr;

		if (SourceLOD)
		{
			if (!HasModuleSlots(SourceLOD, SortedIndices))
			{
				DestroyParticleModules(OwnedDuplicatedModules);
				return;
			}

			TargetModulesByLOD[static_cast<size_t>(TargetLODIndex)] = GetModulesBySortedIndices(SourceLOD, SortedIndices);
			TransferredModules.insert(
				TransferredModules.end(),
				TargetModulesByLOD[static_cast<size_t>(TargetLODIndex)].begin(),
				TargetModulesByLOD[static_cast<size_t>(TargetLODIndex)].end());
		}
		else
		{
			TargetModulesByLOD[static_cast<size_t>(TargetLODIndex)] = DuplicateModules(SourceLOD0Modules);
			if (TargetModulesByLOD[static_cast<size_t>(TargetLODIndex)].empty() && !SourceLOD0Modules.empty())
			{
				DestroyParticleModules(OwnedDuplicatedModules);
				return;
			}
			OwnedDuplicatedModules.insert(
				OwnedDuplicatedModules.end(),
				TargetModulesByLOD[static_cast<size_t>(TargetLODIndex)].begin(),
				TargetModulesByLOD[static_cast<size_t>(TargetLODIndex)].end());
		}
	}

	CaptureUndoSnapshot("MoveModulesToEmitter");

	// source 모든 LOD에서 같은 slot 제거
	for (UParticleLODLevel* SourceLOD : SourceEmitter->LODLevels)
	{
		RemoveModuleSlots(SourceLOD, SortedIndices, TransferredModules);
	}

	// target 모든 LOD 끝에 같은 slot 추가
	for (int32 TargetLODIndex = 0; TargetLODIndex < static_cast<int32>(TargetEmitter->LODLevels.size()); ++TargetLODIndex)
	{
		UParticleLODLevel* TargetLOD = TargetEmitter->LODLevels[static_cast<size_t>(TargetLODIndex)];
		if (!TargetLOD)
		{
			continue;
		}

		TArray<UParticleModule*>& ModulesToAppend = TargetModulesByLOD[static_cast<size_t>(TargetLODIndex)];
		TargetLOD->Modules.insert(TargetLOD->Modules.end(), ModulesToAppend.begin(), ModulesToAppend.end());
	}

	SourceEmitter->CacheEmitterModuleInfo();
	TargetEmitter->CacheEmitterModuleInfo();

	SelectionType = EParticleEditorSelectionType::Module;
	SelectedEmitterIndex = TargetEmitterIndex;
	SelectedLODIndex = 0;
	SelectedModuleIndex = TargetEmitter->LODLevels[0]
		? static_cast<int32>(TargetEmitter->LODLevels[0]->Modules.size()) - static_cast<int32>(SortedIndices.size())
		: -1;

	MarkDirty();
	RestartSimulation();
}

// 특정 Module을 복제하여 지정된 타겟 Emitter의 LOD에 새롭게 추가(복사)합니다.
void FParticleEditorViewer::CopyModuleToEmitter(int32 ModuleIndex, int32 TargetEmitterIndex)
{
	TArray<int32> ModuleIndices;
	ModuleIndices.push_back(ModuleIndex);
	CopyModulesToEmitter(SelectedEmitterIndex, SelectedLODIndex, ModuleIndices, TargetEmitterIndex);
}

void FParticleEditorViewer::CopyModulesToEmitter(int32 SourceEmitterIndex, int32 SourceLODIndex, const TArray<int32>& ModuleIndices, int32 TargetEmitterIndex)
{
	if (!ParticleSystem ||
		SourceEmitterIndex < 0 || SourceEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()) ||
		TargetEmitterIndex < 0 || TargetEmitterIndex >= static_cast<int32>(ParticleSystem->Emitters.size()) ||
		!CanEditLODTopology(SourceLODIndex))
	{
		return;
	}

	UParticleEmitter* SourceEmitter = ParticleSystem->Emitters[SourceEmitterIndex];
	UParticleEmitter* TargetEmitter = ParticleSystem->Emitters[TargetEmitterIndex];
	if (!SourceEmitter || !TargetEmitter || SourceLODIndex < 0 || SourceLODIndex >= static_cast<int32>(SourceEmitter->LODLevels.size()))
	{
		return;
	}

	UParticleLODLevel* SourceLOD = SourceEmitter->LODLevels[SourceLODIndex];
	if (!SourceLOD)
	{
		return;
	}

	const int32 ModuleCount = static_cast<int32>(SourceLOD->Modules.size());
	TArray<int32> SortedIndices = MakeSortedUniqueIndices(ModuleIndices, ModuleCount);
	if (SortedIndices.empty())
	{
		return;
	}

	if (TargetEmitter->LODLevels.empty())
	{
		UParticleLODLevel* TargetLOD0 = CreateDefaultLODLevel(0);
		if (!TargetLOD0)
		{
			return;
		}
		TargetEmitter->LODLevels.push_back(TargetLOD0);
	}

	const TArray<UParticleModule*> SourceLOD0Modules = GetModulesBySortedIndices(SourceLOD, SortedIndices);
	TArray<TArray<UParticleModule*>> CopiedModulesByLOD;
	CopiedModulesByLOD.resize(TargetEmitter->LODLevels.size());
	TArray<UParticleModule*> OwnedDuplicatedModules;
	for (int32 TargetLODIndex = 0; TargetLODIndex < static_cast<int32>(TargetEmitter->LODLevels.size()); ++TargetLODIndex)
	{
		UParticleLODLevel* SameIndexSourceLOD = TargetLODIndex < static_cast<int32>(SourceEmitter->LODLevels.size())
			? SourceEmitter->LODLevels[static_cast<size_t>(TargetLODIndex)]
			: nullptr;
		if (SameIndexSourceLOD && !HasModuleSlots(SameIndexSourceLOD, SortedIndices))
		{
			DestroyParticleModules(OwnedDuplicatedModules);
			return;
		}

		const TArray<UParticleModule*> SourceModulesForLOD = SameIndexSourceLOD
			? GetModulesBySortedIndices(SameIndexSourceLOD, SortedIndices)
			: SourceLOD0Modules;

		CopiedModulesByLOD[static_cast<size_t>(TargetLODIndex)] = DuplicateModules(SourceModulesForLOD);
		if (CopiedModulesByLOD[static_cast<size_t>(TargetLODIndex)].empty() && !SourceModulesForLOD.empty())
		{
			DestroyParticleModules(OwnedDuplicatedModules);
			return;
		}
		OwnedDuplicatedModules.insert(
			OwnedDuplicatedModules.end(),
			CopiedModulesByLOD[static_cast<size_t>(TargetLODIndex)].begin(),
			CopiedModulesByLOD[static_cast<size_t>(TargetLODIndex)].end());
	}

	if (CopiedModulesByLOD.empty() || CopiedModulesByLOD[0].empty())
	{
		DestroyParticleModules(OwnedDuplicatedModules);
		return;
	}

	CaptureUndoSnapshot("CopyModulesToEmitter");

	// target 모든 LOD 끝에 같은 topology slot 추가
	for (int32 TargetLODIndex = 0; TargetLODIndex < static_cast<int32>(TargetEmitter->LODLevels.size()); ++TargetLODIndex)
	{
		UParticleLODLevel* TargetLOD = TargetEmitter->LODLevels[static_cast<size_t>(TargetLODIndex)];
		if (!TargetLOD)
		{
			continue;
		}

		TArray<UParticleModule*>& CopiedModules = CopiedModulesByLOD[static_cast<size_t>(TargetLODIndex)];
		TargetLOD->Modules.insert(TargetLOD->Modules.end(), CopiedModules.begin(), CopiedModules.end());
	}

	TargetEmitter->CacheEmitterModuleInfo();

	SelectedEmitterIndex = TargetEmitterIndex;
	SelectedLODIndex = 0;
	SelectedModuleIndex = TargetEmitter->LODLevels[0]
		? static_cast<int32>(TargetEmitter->LODLevels[0]->Modules.size()) - static_cast<int32>(SortedIndices.size())
		: -1;
	SelectionType = EParticleEditorSelectionType::Module;

	MarkDirty();
	RestartSimulation();
}

// 현재 선택된 Module을 LOD에서 제거 및 메모리 해제 후 선택 인덱스와 시뮬레이션을 갱신합니다.
void FParticleEditorViewer::DeleteSelectedModule()
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	UParticleLODLevel* LOD0 = GetSelectedLODLevel();
	if (!Emitter || !LOD0 || !CanEditSelectedLODTopology())
	{
		return;
	}

	if (SelectedModuleIndex < 0 || SelectedModuleIndex >= static_cast<int32>(LOD0->Modules.size()))
	{
		return;
	}

	TArray<int32> ModuleIndices;
	ModuleIndices.push_back(SelectedModuleIndex);
	const TArray<int32> SortedIndices = MakeSortedUniqueIndices(ModuleIndices, static_cast<int32>(LOD0->Modules.size()));
	if (SortedIndices.empty())
	{
		return;
	}
	for (UParticleLODLevel* LOD : Emitter->LODLevels)
	{
		if (!HasModuleSlots(LOD, SortedIndices))
		{
			return;
		}
	}

	CaptureUndoSnapshot("DeleteModule");

	// 모든 LOD의 같은 topology slot 제거
	TArray<UParticleModule*> TransferredModules;
	for (UParticleLODLevel* LOD : Emitter->LODLevels)
	{
		RemoveModuleSlots(LOD, SortedIndices, TransferredModules);
	}

	SelectedModuleIndex = LOD0->Modules.empty()
							  ? -1
							  : std::clamp(SelectedModuleIndex, 0, static_cast<int32>(LOD0->Modules.size()) - 1);
	SelectionType = SelectedModuleIndex >= 0
						? EParticleEditorSelectionType::Module
						: EParticleEditorSelectionType::LODLevel;

	RefreshEmitterAfterTopologyEdit(Emitter);
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

	UParticleLODLevel* LOD = CreateLODFromLOD0Topology(Emitter, static_cast<int32>(Emitter->LODLevels.size()));
	if (!LOD)
	{
		return;
	}

	CaptureUndoSnapshot("AddLOD");

	Emitter->LODLevels.push_back(LOD);
	SyncParticleSystemLODDistancesToLODCount(ParticleSystem);

	SelectionType = EParticleEditorSelectionType::LODLevel;
	SelectedLODIndex = static_cast<int32>(Emitter->LODLevels.size()) - 1;
	SelectedModuleIndex = -1;

	RefreshEmitterAfterTopologyEdit(Emitter);
}

// 현재 선택된 Emitter에서 지정한 인덱스의 LOD 레벨을 제거하고 메모리를 해제합니다 (최소 1개 이상은 유지).
void FParticleEditorViewer::RemoveLOD(int32 LODIndex)
{
	UParticleEmitter* Emitter = GetSelectedEmitter();
	if (!Emitter || LODIndex < 0 || LODIndex >= static_cast<int32>(Emitter->LODLevels.size()))
	{
		return;
	}

	if (Emitter->LODLevels.size() <= 1 || LODIndex == 0)
	{
		return;
	}

	CaptureUndoSnapshot("RemoveLOD");

	UParticleLODLevel* RemovedLOD = Emitter->LODLevels[LODIndex];
	Emitter->LODLevels.erase(Emitter->LODLevels.begin() + LODIndex);
	UObjectManager::Get().DestroyObject(RemovedLOD);
	SyncParticleSystemLODDistancesToLODCount(ParticleSystem);

	SelectedLODIndex = std::clamp(SelectedLODIndex, 0, static_cast<int32>(Emitter->LODLevels.size()) - 1);
	SelectedModuleIndex = -1;

	RefreshEmitterAfterTopologyEdit(Emitter);
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

const UParticleLODLevel* FParticleEditorViewer::FindTrailPreviewLOD() const
{
	const UParticleLODLevel* SelectedLOD = GetSelectedLODLevel();
	if (SelectedLOD && Cast<UParticleModuleTypeDataTrailBase>(SelectedLOD->TypeDataModule))
	{
		return SelectedLOD;
	}

	if (!ParticleSystem)
	{
		return nullptr;
	}

	for (const UParticleEmitter* Emitter : ParticleSystem->Emitters)
	{
		if (!Emitter)
		{
			continue;
		}

		for (const UParticleLODLevel* LOD : Emitter->LODLevels)
		{
			if (LOD && Cast<UParticleModuleTypeDataTrailBase>(LOD->TypeDataModule))
			{
				return LOD;
			}
		}
	}

	return nullptr;
}

void FParticleEditorViewer::UpdateTrailPreviewMotion(float DeltaTime)
{
	if (!PreviewActor || !UObjectManager::Get().ContainsObject(PreviewActor) ||
		!PreviewComponent || !UObjectManager::Get().ContainsObject(PreviewComponent))
	{
		TrailPreviewTime = 0.0f;
		return;
	}

	const UParticleLODLevel* LOD = FindTrailPreviewLOD();
	if (!LOD)
	{
		TrailPreviewTime = 0.0f;
		PreviewComponent->SetRelativeLocation(FVector::ZeroVector);
		return;
	}

	constexpr float RotationSpeedRadiansPerSecond = 1.57079632679f;
	const float Radius = GetTrailPreviewRadius(LOD);
	TrailPreviewTime += std::max(0.0f, DeltaTime);
	const float Angle = TrailPreviewTime * RotationSpeedRadiansPerSecond;

	PreviewComponent->SetRelativeLocation(FVector(
		std::cos(Angle) * Radius,
		std::sin(Angle) * Radius,
		0.0f));
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
	TrailPreviewTime = 0.0f;

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

	PreviewActor = World->SpawnActor<AParticleSystemActor>();
	if (!PreviewActor)
	{
		return false;
	}

	PreviewWorld = World;

	PreviewActor->SetFName(FName("ParticleViewerActor"));
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));
	PreviewActor->InitDefaultComponents();

	PreviewComponent = PreviewActor->GetParticleSystemComponent();
	if (!PreviewComponent)
	{
		ClearParticlePreview();
		return false;
	}

	PreviewComponent->SetTemplate(ParticleSystem);
	UpdateTrailPreviewMotion(0.0f);
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
	SyncParticleSystemLODDistancesToLODCount(ParticleSystem);

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

	SyncParticleSystemLODDistancesToLODCount(ParticleSystem);

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

UParticleLODLevel* FParticleEditorViewer::CreateLODFromLOD0Topology(UParticleEmitter* Emitter, int32 Level)
{
	if (!Emitter || Emitter->LODLevels.empty() || !Emitter->LODLevels[0])
	{
		return CreateDefaultLODLevel(Level);
	}

	UParticleLODLevel* SourceLOD0 = Emitter->LODLevels[0];
	UParticleLODLevel* NewLOD = NewObject<UParticleLODLevel>();
	if (!NewLOD)
	{
		return nullptr;
	}

	NewLOD->Level = Level;
	NewLOD->bEnabled = SourceLOD0->bEnabled;

	auto CleanupNewLOD = [NewLOD]()
	{
		// 부분 생성된 module 정리
		DestroyParticleModuleIfLive(NewLOD->RequiredModule);
		DestroyParticleModuleIfLive(NewLOD->SpawnModule);
		DestroyParticleModuleIfLive(NewLOD->TypeDataModule);
		TArray<UParticleModule*> Modules = NewLOD->Modules;
		DestroyParticleModules(Modules);
		NewLOD->Modules.clear();
		UObjectManager::Get().DestroyObject(NewLOD);
	};

	// 특수 module slot 복제
	UParticleModule* CopiedRequiredModule = DuplicateParticleModule(SourceLOD0->RequiredModule);
	if (SourceLOD0->RequiredModule && !CopiedRequiredModule)
	{
		CleanupNewLOD();
		return nullptr;
	}
	NewLOD->RequiredModule = Cast<UParticleModuleRequired>(CopiedRequiredModule);
	if (CopiedRequiredModule && !NewLOD->RequiredModule)
	{
		DestroyParticleModuleIfLive(CopiedRequiredModule);
		CleanupNewLOD();
		return nullptr;
	}

	UParticleModule* CopiedSpawnModule = DuplicateParticleModule(SourceLOD0->SpawnModule);
	if (SourceLOD0->SpawnModule && !CopiedSpawnModule)
	{
		CleanupNewLOD();
		return nullptr;
	}
	NewLOD->SpawnModule = Cast<UParticleModuleSpawn>(CopiedSpawnModule);
	if (CopiedSpawnModule && !NewLOD->SpawnModule)
	{
		DestroyParticleModuleIfLive(CopiedSpawnModule);
		CleanupNewLOD();
		return nullptr;
	}

	UParticleModule* CopiedTypeDataModule = DuplicateParticleModule(SourceLOD0->TypeDataModule);
	if (SourceLOD0->TypeDataModule && !CopiedTypeDataModule)
	{
		CleanupNewLOD();
		return nullptr;
	}
	NewLOD->TypeDataModule = Cast<UParticleModuleTypeDataBase>(CopiedTypeDataModule);
	if (CopiedTypeDataModule && !NewLOD->TypeDataModule)
	{
		DestroyParticleModuleIfLive(CopiedTypeDataModule);
		CleanupNewLOD();
		return nullptr;
	}

	// 일반 module topology 복제
	NewLOD->Modules = DuplicateModules(SourceLOD0->Modules);
	if (NewLOD->Modules.empty() && !SourceLOD0->Modules.empty())
	{
		CleanupNewLOD();
		return nullptr;
	}

	return NewLOD;
}

void FParticleEditorViewer::RefreshEmitterAfterTopologyEdit(UParticleEmitter* Emitter)
{
	if (Emitter)
	{
		// runtime module cache 갱신
		Emitter->CacheEmitterModuleInfo();
	}
	SyncParticleSystemLODDistancesToLODCount(ParticleSystem);

	// 편집 상태와 preview simulation 갱신
	MarkDirty();
	RestartSimulation();
}
