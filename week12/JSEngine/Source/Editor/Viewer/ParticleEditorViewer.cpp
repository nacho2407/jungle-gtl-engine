#include "ParticleEditorViewer.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleSystemComponent.h"
#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"

#include <algorithm>

// 파일명 저장 → 기존 Selection 정리 → ParticleSystem Asset 로드 → PreviewActor의 ParticleSystemComponent에 템플릿 설정 → 시뮬레이션 재시작
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

void FParticleEditorViewer::Tick(float DeltaTime)
{
	FEditorViewer::Tick(DeltaTime);

	if (PreviewComponent && UObjectManager::Get().ContainsObject(PreviewComponent) && IsPlaying() && IsRealtime())
	{
		PreviewComponent->TickComponent(DeltaTime);
	}
}

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

void FParticleEditorViewer::RestartLevel()
{
	RestartSimulation();
}

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

void FParticleEditorViewer::SetRealtime(bool bInRealtime)
{
	Client.SetRealtime(bInRealtime);
}

void FParticleEditorViewer::SetViewMode(EViewMode InViewMode)
{
	Client.SetViewMode(InViewMode);
}

EViewMode FParticleEditorViewer::GetViewMode() const
{
	return Client.GetViewMode();
}

// Emitter 추가 → 기본 LOD + RequiredModule 같이 세팅 → 시뮬레이션 재시작
void FParticleEditorViewer::AddEmitter()
{
	if (!ParticleSystem)
	{
		return;
	}

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

// 왼쪽 클릭 + 드래그로 Emitter 이동
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

// 오른쪽 클릭으로 Emitter Context Menu에 접근해 Module 추가
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
		// Spawn rate provider는 LOD의 특수 모듈이므로 일반 Modules 배열에 넣지 않음!
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

// 왼쪽 클릭 드래그로 Source Emitter에서 Target Emitter로 모듈 이동
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

// Ctrl + 드래그로 Source Emitter에서 Target Emitter로 모듈 복사
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

// Delete 키로 선택된 모듈 삭제
void FParticleEditorViewer::DeleteSelectedModule()
{
	UParticleLODLevel* LOD = GetSelectedLODLevel();
	if (!LOD || SelectedModuleIndex < 0 || SelectedModuleIndex >= static_cast<int32>(LOD->Modules.size()))
	{
		return;
	}

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

bool FParticleEditorViewer::Save()
{
	// TODO: Particle Serializer 연결 전까지 Fals 반환
	return false;
}

bool FParticleEditorViewer::SaveAs(const FString& InFileName)
{
	if (InFileName.empty())
	{
		return false;
	}

	SetFileName(FPaths::Normalize(InFileName));
	return Save();
}

void FParticleEditorViewer::FindInContentBrowser()
{
	const FString& CurrentFileName = GetFileName();
	(void)CurrentFileName;
	// TODO: Content Browser service 연결 후 현재 ParticleSystem asset 선택
}

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

	Emitter->LODLevels.push_back(LOD);
	Emitter->CacheEmitterModuleInfo();

	SelectionType = EParticleEditorSelectionType::LODLevel;
	SelectedLODIndex = static_cast<int32>(Emitter->LODLevels.size()) - 1;
	SelectedModuleIndex = -1;

	MarkDirty();
	RestartSimulation();
}

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

	UParticleLODLevel* RemovedLOD = Emitter->LODLevels[LODIndex];
	Emitter->LODLevels.erase(Emitter->LODLevels.begin() + LODIndex);
	UObjectManager::Get().DestroyObject(RemovedLOD);
	Emitter->CacheEmitterModuleInfo();

	SelectedLODIndex = std::clamp(SelectedLODIndex, 0, static_cast<int32>(Emitter->LODLevels.size()) - 1);
	SelectedModuleIndex = -1;

	MarkDirty();
	RestartSimulation();
}

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

void FParticleEditorViewer::ClearParticleSelection()
{
	SelectedEmitterIndex = -1;
	SelectedLODIndex = -1;
	SelectedModuleIndex = -1;
	SelectionType = EParticleEditorSelectionType::None;
}

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

bool FParticleEditorViewer::LoadParticleSystemAsset(const FString& InFileName)
{
	(void)InFileName;
	ParticleSystem = nullptr;
	bOwnsParticleSystem = false;

	if (InFileName.empty())
	{
		return false;
	}

	// TODO: Particle Serializer 연결 전까지 기본 ParticleSystem 생성
	// ParticleSystem = FResourceManager::Get().LoadParticleSystem(InFileName);

	return false;
}

void FParticleEditorViewer::EnsureDefaultParticleSystem()
{
	if (!ParticleSystem)
	{
		ParticleSystem = NewObject<UParticleSystem>();
		bOwnsParticleSystem = true;
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
