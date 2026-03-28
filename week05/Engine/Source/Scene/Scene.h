#pragma once

#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include <d3d11.h>
#include "WorldTypes.h"
#include "Core/ShowFlags.h"
#include "RenderCollector.h"
class AActor;
class FCamera;
class FFrustum;
class UCameraComponent;
class UPrimitiveComponent;
struct FRenderCommandQueue;

class ENGINE_API UScene : public UObject
{
public:
	DECLARE_RTTI(UScene, UObject)
	~UScene();

	template <typename T>
	T* SpawnActor(const FString& InName)
	{
		static_assert(std::is_base_of_v<AActor, T>, "T must derive from AActor");

		T* NewActor = FObjectFactory::ConstructObject<T>(this, InName);
		if (!NewActor)
		{
			return nullptr;
		}
		RegisterActor(NewActor);
		NewActor->PostSpawnInitialize();

		return NewActor;
	}

	void RegisterActor(AActor* InActor);
	void DestroyActor(AActor* InActor);
	void CleanupDestroyedActors();

	const TArray<AActor*>& GetActors() const { return Actors; }
	EWorldType GetWorldType() const;
	bool IsEditorScene() const;
	bool IsGameScene() const;

  
	FCamera* GetCamera() const;


	void ClearActors();
	void BeginPlay();
	void Tick(float DeltaTime);

	
	

private:
	TArray<AActor*> Actors;
	bool bBegunPlay = false;
};
