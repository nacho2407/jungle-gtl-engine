#include "World.h"
#include "Object/Class.h"  
#include "Level/Level.h"
#include "Object/ObjectFactory.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"
#include "Serializer/SceneSerializer.h"
#include "Core/Paths.h"
#include "Actor/Actor.h"
#include <filesystem>
#include <initializer_list>

namespace
{
	bool TryLoadSceneFromCandidates(ULevel* Scene, ID3D11Device* Device, const std::initializer_list<std::filesystem::path>& CandidatePaths)
	{
		if (!Scene || !Device)
		{
			return false;
		}

		for (const std::filesystem::path& CandidatePath : CandidatePaths)
		{
			if (!std::filesystem::exists(CandidatePath))
			{
				continue;
			}

			if (FSceneSerializer::Load(Scene, FPaths::FromPath(CandidatePath), Device))
			{
				return true;
			}
		}

		return false;
	}
}

IMPLEMENT_RTTI(UWorld, UObject)

UWorld::~UWorld()
{
	CleanupWorld();
}

void UWorld::InitializeWorld(float AspectRatio, ID3D11Device* Device)
{
	PersistentLevel = FObjectFactory::ConstructObject<ULevel>(this, "PersistentLevel");
	if (!PersistentLevel)
	{
		return;
	}

	if (!SceneCameraComponent)
	{
		SceneCameraComponent = FObjectFactory::ConstructObject<UCameraComponent>(this, "SceneCamera");
	}
	if (!ActiveCameraComponent)
	{
		ActiveCameraComponent = SceneCameraComponent;
	}
	if (SceneCameraComponent->GetCamera())
	{
		SceneCameraComponent->GetCamera()->SetAspectRatio(AspectRatio);
	}

	if (Device)
	{
		const std::filesystem::path SceneDir = FPaths::SceneDir();
		TryLoadSceneFromCandidates(PersistentLevel, Device, {
			SceneDir / "DefaultScene.json",
			SceneDir / "Default.scene"
		});
	}
}

void UWorld::BeginPlay()
{
	if (bBegunPlay)
	{
		return;
	}

	bBegunPlay = true;

	if (PersistentLevel)
	{
		for (AActor* Actor : PersistentLevel->GetActors())
		{
			if (Actor && !Actor->HasBegunPlay())
			{
				Actor->BeginPlay();
			}
		}
	}

	// [Minjun][deprecated] StreamingLevels는 삭제될 예정
	//for (ULevel* Level : StreamingLevels)
	//{
	//	if (Level)
	//	{
	//		for (AActor* Actor : Level->GetActors())
	//		{
	//			if (Actor && !Actor->HasBegunPlay())
	//			{
	//				Actor->BeginPlay();
	//			}
	//		}
	//	}
	//}
}

void UWorld::EndPlay()
{
	if (!bBegunPlay)
	{
		return;
	}

	if (PersistentLevel)
	{
		for (AActor* Actor : PersistentLevel->GetActors())
		{
			if (Actor && Actor->HasBegunPlay() && !Actor->IsPendingDestroy())
			{
				Actor->EndPlay();
			}
		}
	}

	// [Minjun][deprecated] StreamingLevels는 삭제될 예정
	//for (ULevel* Level : StreamingLevels)
	//{
	//	if (Level)
	//	{
	//		for (AActor* Actor : Level->GetActors())
	//		{
	//			if (Actor && Actor->HasBegunPlay() && !Actor->IsPendingDestroy())
	//			{
	//				Actor->EndPlay();
	//			}
	//		}
	//	}
	//}

	bBegunPlay = false;
}

void UWorld::Tick(float InDeltaTime)
{
	DeltaSeconds = InDeltaTime;
	WorldTime += InDeltaTime;

	if (PersistentLevel)
	{
		const EWorldType CurrentWorldType = GetWorldType();
		const bool bShouldTickForPlay = (CurrentWorldType == EWorldType::Game || CurrentWorldType == EWorldType::PIE) && bBegunPlay;
		const bool bShouldTickForEditor = (CurrentWorldType == EWorldType::Editor);

		for (AActor* Actor : PersistentLevel->GetActors())
		{
			if (!Actor || Actor->IsPendingDestroy())
			{
				continue;
			}

			if (bShouldTickForEditor)
			{
				if (!Actor->IsTickInEditor())
				{
					continue;
				}
			}
			else if (bShouldTickForPlay)
			{
				if (!Actor->HasBegunPlay())
				{
					continue;
				}
			}
			else
			{
				// 나중에 월드 타입이 늘어나면 여기서 추가적으로 체크
				continue;
			}

			Actor->Tick(InDeltaTime);
		}

		PersistentLevel->CleanupDestroyedActors();
	}

	// [Minjun][deprecated] StreamingLevels는 삭제될 예정
	//for (ULevel* Level : StreamingLevels)
	//{
	//	if (Level)
	//	{
	//		Level->Tick(InDeltaTime);
	//	}
	//}
}

void UWorld::ResetRuntimeState()
{
	bBegunPlay = false;
	WorldTime = 0.f;
	DeltaSeconds = 0.f;
	ActiveCameraComponent = SceneCameraComponent;
}

void UWorld::CleanupWorld()
{
	for (ULevel* Level : StreamingLevels)
	{
		if (Level)
		{
			Level->ClearActors();
			Level->MarkPendingKill();
		}
	}
	if (PersistentLevel)
	{
		PersistentLevel->ClearActors();
		PersistentLevel->MarkPendingKill();
		PersistentLevel = nullptr;
	}
	if (SceneCameraComponent)
	{
		SceneCameraComponent->MarkPendingKill();
	}
	if (ActiveCameraComponent == SceneCameraComponent)
	{
		ActiveCameraComponent = nullptr;
	}
	SceneCameraComponent = nullptr;
	WorldTime = 0.f;
	DeltaSeconds = 0.f;
}

UWorld* UWorld::DuplicateWorldForPIE(UWorld* EditorWorld)
{
	if (!EditorWorld || !EditorWorld->PersistentLevel)
	{
		return nullptr;
	}
	// Play-In-Editor World를 생성한다.
	UWorld* PIEWorld = FObjectFactory::ConstructObject<UWorld>(nullptr, EditorWorld->GetName() + "_PIE", EObjectFlags::Transient);

	if (!PIEWorld)
	{
		return nullptr;
	}
	
	// DuplicatedObjects로 Object 매핑 관계를 관리하는 Context
	FDuplicateContext Context;
	Context.DuplicateFlags = EObjectFlags::Transient;

	auto FailDuplicate = [PIEWorld]() -> UWorld*
	{
		if (PIEWorld)
		{
			PIEWorld->CleanupWorld();
			PIEWorld->MarkPendingKill();
		}
		return nullptr;
	};
	
	// PIE World 초기 설정
	PIEWorld->WorldType = EWorldType::PIE;
	PIEWorld->bBegunPlay = false;
	PIEWorld->WorldTime = 0.0f;
	PIEWorld->DeltaSeconds = 0.0f;

	// PIEWorld에 EditorWorld의 PersistentLevel Duplicate(깊은 복사) 수행
	PIEWorld->PersistentLevel = static_cast<ULevel*>(
		EditorWorld->PersistentLevel->Duplicate(PIEWorld, EditorWorld->PersistentLevel->GetName(), Context));

	if (!PIEWorld->PersistentLevel)
	{
		return FailDuplicate();
	}

	// Duplicate 이후 참조 재연결 DuplicateCotext 사용
	EditorWorld->PersistentLevel->FixupDuplicatedReferences(PIEWorld->PersistentLevel, Context);
	// PIE Objects 후처리 (register 재등록)
	EditorWorld->PersistentLevel->PostDuplicate(PIEWorld->PersistentLevel, Context);

	PIEWorld->StreamingLevels.clear();

	return PIEWorld;
}

void UWorld::DestroyActor(AActor* InActor)
{
	if (!InActor || !PersistentLevel) return;


	if (ActiveCameraComponent && ActiveCameraComponent != SceneCameraComponent)
	{
		for (UActorComponent* Component : InActor->GetComponents())
		{
			if (Component == ActiveCameraComponent)
			{
				ActiveCameraComponent = SceneCameraComponent;
				break;
			}
		}
	}

	PersistentLevel->DestroyActor(InActor);
}

ULevel* UWorld::LoadStreamingLevel(const FString& LevelName, ID3D11Device* Device)
{
	(void)LevelName;
	(void)Device;

	// StreamingLevel은 현재 프로젝트에서 사용하지 않으므로 비활성화

	return nullptr;
}

void UWorld::UnloadStreamingLevel(const FString& LevelName)
{
	(void)LevelName;
}

ULevel* UWorld::FindStreamingLevel(const FString& LevelName) const
{
	(void)LevelName;
	return nullptr;
}

TArray<AActor*> UWorld::GetAllActors() const
{
	TArray<AActor*> AllActors;
	if (PersistentLevel)
	{
		const auto& PersistentActors = PersistentLevel->GetActors();
		AllActors.insert(AllActors.end(), PersistentActors.begin(), PersistentActors.end());
	}
	for (ULevel* Level : StreamingLevels)
	{
		if (Level)
		{
			const auto& LevelActors = Level->GetActors();
			AllActors.insert(AllActors.end(), LevelActors.begin(), LevelActors.end());
		}
	}
	return AllActors;
}

const TArray<AActor*>& UWorld::GetActors() const
{
	static TArray<AActor*> EmptyArray;
	if (PersistentLevel)
	{
		return PersistentLevel->GetActors();
	}
	return EmptyArray;
}

void UWorld::SetActiveCameraComponent(UCameraComponent* InCamera)
{
	ActiveCameraComponent = InCamera ? InCamera : SceneCameraComponent;
}

UCameraComponent* UWorld::GetActiveCameraComponent() const
{
	return ActiveCameraComponent ? ActiveCameraComponent.Get() : SceneCameraComponent;
}

FCamera* UWorld::GetCamera() const
{
	UCameraComponent* Cam = GetActiveCameraComponent();
	return Cam ? Cam->GetCamera() : nullptr;
}
