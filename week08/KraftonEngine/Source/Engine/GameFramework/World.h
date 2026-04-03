#pragma once
#include "Object/Object.h"
#include "GameFramework/AActor.h"
#include "Render/Proxy/FScene.h"

class UCameraComponent;

class UWorld : public UObject {
public:
	DECLARE_CLASS(UWorld, UObject)
	UWorld() = default;
	~UWorld() override;

	// Actor lifecycle
	template<typename T>
	T* SpawnActor();
	void DestroyActor(AActor* Actor);

	const TArray<AActor*>& GetActors() const { return Actors; }
	void AddActor(AActor* Actor) { Actors.push_back(Actor); }

	void InitWorld();      // Set up the world before gameplay begins
	void BeginPlay();      // Triggers BeginPlay on all actors
	void Tick(float DeltaTime);  // Drives the game loop every frame
	void EndPlay();        // Cleanup before world is destroyed

	bool HasBegunPlay() const { return bHasBegunPlay; }

	// Active Camera — EditorViewportClient 또는 PlayerController가 세팅
	void SetActiveCamera(UCameraComponent* InCamera) { ActiveCamera = InCamera; }
	UCameraComponent* GetActiveCamera() const { return ActiveCamera; }

	// FScene — 렌더 프록시 관리자
	FScene& GetScene() { return Scene; }
	const FScene& GetScene() const { return Scene; }

private:
	TArray<AActor*> Actors;
	UCameraComponent* ActiveCamera = nullptr;
	bool bHasBegunPlay = false;
	FScene Scene;
};

template<typename T>
inline T* UWorld::SpawnActor()
{
	// create and register an actor
	T* Actor = UObjectManager::Get().CreateObject<T>();
	Actor->SetWorld(this);
	if (bHasBegunPlay)
	{
		Actor->BeginPlay();
	}
	Actors.push_back(Actor);
	return Actor;
}
