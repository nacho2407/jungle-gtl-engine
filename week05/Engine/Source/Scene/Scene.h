#pragma once

#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include <d3d11.h>
#include "SceneTypes.h"
#include "Core/ShowFlags.h"
#include "RenderCollector.h"
class AActor;
class CCamera;
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
	void SetSceneType(ESceneType InSceneType) { SceneType = InSceneType; }
	ESceneType GetSceneType() const { return SceneType; }
	bool IsEditorScene() const { return SceneType == ESceneType::Editor; }
	bool IsGameScene() const { return SceneType == ESceneType::Game || SceneType == ESceneType::PIE; }

  
	CCamera* GetCamera() const;


	void ClearActors();
	void BeginPlay();
	void Tick(float DeltaTime);

	
	

private:
	TArray<AActor*> Actors;
	bool bBegunPlay = false;
	ESceneType SceneType = ESceneType::Game;


};
