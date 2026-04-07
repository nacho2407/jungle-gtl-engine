#include "World.h"
#include "Object/Class.h"  
#include "Scene/Scene.h"
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
	bool TryLoadSceneFromCandidates(UScene* Scene, ID3D11Device* Device, const std::initializer_list<std::filesystem::path>& CandidatePaths)
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
	PersistentLevel = FObjectFactory::ConstructObject<UScene>(this, "PersistentLevel");
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
	if (bBegunPlay) return;  
	bBegunPlay = true;     
	if (PersistentLevel)
	{
		PersistentLevel->BeginPlay();
	}
	for (UScene* Level : StreamingLevels)
	{
		if (Level) Level->BeginPlay();
	}
}

void UWorld::Tick(float InDeltaTime)
{
	DeltaSeconds = InDeltaTime;
	WorldTime += InDeltaTime;

	if (PersistentLevel)
	{
		PersistentLevel->Tick(InDeltaTime);
	}
	for (UScene* Level : StreamingLevels)
	{
		if (Level)
		{
			Level->Tick(InDeltaTime);
		}
	}
}

void UWorld::CleanupWorld()
{
	for (UScene* Level : StreamingLevels)
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
	return nullptr;
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

UScene* UWorld::LoadStreamingLevel(const FString& LevelName, ID3D11Device* Device)
{
	// 이미 로드됐는지 확인
	if (UScene* Existing = FindStreamingLevel(LevelName))
	{
		return Existing;
	}
	UScene* NewLevel = FObjectFactory::ConstructObject<UScene>(this, LevelName);
	if (!NewLevel) return nullptr;

	if (Device)
	{
		const std::filesystem::path SceneDir = FPaths::SceneDir();
		TryLoadSceneFromCandidates(NewLevel, Device, {
			SceneDir / FPaths::ToPath(LevelName + ".json"),
			SceneDir / FPaths::ToPath(LevelName + ".scene")
		});
	}
	StreamingLevels.push_back(NewLevel);

	// 이미 게임 진행 중이면 BeginPlay 호출
	if (bBegunPlay)
	{
		NewLevel->BeginPlay();
	}
	return NewLevel;
}

void UWorld::UnloadStreamingLevel(const FString& LevelName)
{
	auto It = std::find_if(StreamingLevels.begin(), StreamingLevels.end(),
		[&](UScene* Level) { return Level->GetName() == LevelName; });
	if (It != StreamingLevels.end())
	{
		(*It)->ClearActors();
		(*It)->MarkPendingKill();
		StreamingLevels.erase(It);
	}
}

UScene* UWorld::FindStreamingLevel(const FString& LevelName) const
{
	for (UScene* Level : StreamingLevels)
	{
		if (Level && Level->GetName() == LevelName)
		{
			return Level;
		}
	}
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
	for (UScene* Level : StreamingLevels)
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
