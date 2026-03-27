#include "Scene.h"

#include "Core/Paths.h"
#include "Actor/Actor.h"
#include "Actor/AttachTestActor.h"
#include "Actor/CubeActor.h"
#include "Actor/SphereActor.h"
#include "Actor/SubUVActor.h"
#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Object/ObjectFactory.h"
#include "Component/PrimitiveComponent.h"
#include "Object/Class.h"

#include "Serializer/SceneSerializer.h"
#include <algorithm>



#include "Component/LineBatchComponent.h"

IMPLEMENT_RTTI(UScene, UObject)

UScene::~UScene()
{
	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	Actors.clear();


}


CCamera* UScene::GetCamera() const
{
	UWorld* World = GetTypedOuter<UWorld>();
	return World ? World->GetCamera() : nullptr;
}



void UScene::ClearActors()
{


	for (AActor* Actor : Actors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	Actors.clear();

	bBegunPlay = false;
}

void UScene::RegisterActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}

	const auto It = std::find(Actors.begin(), Actors.end(), InActor);
	if (It != Actors.end())
	{
		return;
	}

	Actors.push_back(InActor);
	InActor->SetScene(this);
}

void UScene::DestroyActor(AActor* InActor)
{
	if (!InActor)
	{
		return;
	}


	InActor->Destroy();
}

void UScene::CleanupDestroyedActors()
{
	const auto NewEnd = std::ranges::remove_if(Actors,
		[](const AActor* Actor)
		{
			return Actor == nullptr || Actor->IsPendingDestroy();
		}).begin();

	Actors.erase(NewEnd, Actors.end());
}

void UScene::BeginPlay()
{
	if (bBegunPlay)
	{
		return;
	}

	bBegunPlay = true;

	for (AActor* Actor : Actors)
	{
		if (Actor && !Actor->HasBegunPlay())
		{
			Actor->BeginPlay();
		}
	}
}

void UScene::Tick(float DeltaTime)
{
	if (!bBegunPlay)
	{
		BeginPlay();
	}

	for (AActor* Actor : Actors)
	{
		if (Actor && !Actor->IsPendingDestroy())
		{
			Actor->Tick(DeltaTime);
		}
	}

	CleanupDestroyedActors();
}
