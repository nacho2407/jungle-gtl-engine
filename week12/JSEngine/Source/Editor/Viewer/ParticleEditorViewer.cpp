#include "ParticleEditorViewer.h"
#include "Particle/ParticleAsset.h"
#include "Particle/ParticleSystemComponent.h"
#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"

// 파일명 저장 → 기존 Selection 정리 → ParticleSystem Asset 로드 → PreviewActor의 ParticleSystemComponent에 템플릿 설정 → 시뮬레이션 재시작
bool FParticleEditorViewer::ChangeTarget(const FString& InFileName)
{
	SetFileName(InFileName);
	ClearBaseSelection();
	ClearParticleSelection();
	ClearParticlePreview();

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

	if (PreviewComponent && IsPlaying())
	{
		PreviewComponent->TickComponent(DeltaTime);
	}
}

void FParticleEditorViewer::Shutdown()
{
	ClearParticlePreview();
	ClearParticleSelection();
	ParticleSystem = nullptr;
	FEditorViewer::Shutdown();
}

void FParticleEditorViewer::RestartSimulation()
{
	if (!PreviewComponent || !ParticleSystem)
		return;

	if (PreviewComponent)
	{
		PreviewComponent->SetTemplate(ParticleSystem);
		PreviewComponent->ResetParticles();
	}
}

void FParticleEditorViewer::SelectEmitter(int32 EmitterIndex)
{
	SelectedEmitterIndex = EmitterIndex;
	SelectedLODIndex = 0;
	SelectedModuleIndex = -1;
}

void FParticleEditorViewer::SelectLOD(int32 LODIndex)
{
	SelectedLODIndex = LODIndex;
	SelectedModuleIndex = -1;
}

void FParticleEditorViewer::SelectModule(int32 ModuleIndex)
{
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


UObject* FParticleEditorViewer::GetSelectedObject() const
{
	if (UParticleModule* Module = GetSelectedModule())
	{
		return Module;
	}

	if (UParticleLODLevel* LODLevel = GetSelectedLODLevel())
	{
		return LODLevel;
	}

	if (UParticleEmitter* Emitter = GetSelectedEmitter())
	{
		return Emitter;
	}

	return ParticleSystem;
}

// Emitter 추가 → 기본 LOD + RequiredModule 같이 세팅 → 시뮬레이션 재시작
void FParticleEditorViewer::AddEmitter()
{
	if (!ParticleSystem)
	{
		return;
	}

	UParticleEmitter* Emitter = NewObject<UParticleEmitter>();
	UParticleLODLevel* LOD = NewObject<UParticleLODLevel>();

	LOD->Level = 0;
	LOD->bEnabled = true;
	LOD->RequiredModule = NewObject<UParticleModuleRequired>();
	LOD->TypeDataModule = NewObject<UParticleModuleTypeDataBase>();

	Emitter->LODLevels.push_back(LOD);
	Emitter->CacheEmitterModuleInfo();

	ParticleSystem->Emitters.push_back(Emitter);

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

	ParticleSystem->Emitters.erase(ParticleSystem->Emitters.begin() + SelectedEmitterIndex);
	ClearParticleSelection();

	MarkDirty();
	RestartSimulation();
}

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

	LOD->Modules.push_back(Module);
	SelectedModuleIndex = static_cast<int32>(LOD->Modules.size()) - 1;

	MarkDirty();
	RestartSimulation();
}

void FParticleEditorViewer::ClearParticlePreview()
{
	if (PreviewComponent)
	{
		PreviewComponent->SetTemplate(nullptr);
		PreviewComponent->ResetParticles();
	}
}

void FParticleEditorViewer::ClearParticleSelection()
{
	SelectedEmitterIndex = -1;
	SelectedLODIndex = 0;
	SelectedModuleIndex = -1;
}

bool FParticleEditorViewer::CreatePreviewComponent()
{
	if (PreviewComponent)
	{
		PreviewComponent->SetTemplate(ParticleSystem);
		return true;
	}

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

	PreviewActor->SetFName(FName("ParticleViewerActor"));
	PreviewActor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	PreviewComponent = PreviewActor->AddComponent<UParticleSystemComponent>();
	if (!PreviewComponent)
	{
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

	if (InFileName.empty())
	{
		return false;
	}

	// ParticleSystem = FResourceManager::Get().LoadParticleSystem(InFileName);

	return false;
}

void FParticleEditorViewer::EnsureDefaultParticleSystem()
{
	if (!ParticleSystem)
	{
		ParticleSystem = NewObject<UParticleSystem>();
	}

	if (ParticleSystem->Emitters.empty())
	{
		AddEmitter();
	}
}
