#include "World.h"
#include "Object/Class.h"  
#include "Scene/Scene.h"
#include "Object/ObjectFactory.h"
#include "Component/CameraComponent.h"
#include "Camera/Camera.h"
#include "Serializer/SceneSerializer.h"
#include "Core/Paths.h"
#include "Actor/Actor.h"
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

	PersistentLevel->SetSceneType(WorldType);

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
		FSceneSerializer::Load(PersistentLevel, (FPaths::SceneDir() / "DefaultScene.json").string(), Device);
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
	NewLevel->SetSceneType(WorldType);

	if (Device)
	{
		FSceneSerializer::Load(NewLevel, (FPaths::SceneDir() / (LevelName + ".json")).string(), Device);
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

CCamera* UWorld::GetCamera() const
{
	UCameraComponent* Cam = GetActiveCameraComponent();
	return Cam ? Cam->GetCamera() : nullptr;
}

