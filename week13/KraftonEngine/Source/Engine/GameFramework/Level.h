#pragma once
#include "Object/Object.h"
#include "Object/Ptr/ObjectPtr.h"
#include "Object/Ptr/WeakObjectPtr.h"
#include "GameFramework/AActor.h"
#include "Source/Engine/GameFramework/Level.generated.h"
#include <memory>

class AActor;
class UWorld;
class FSpatialPartition;

UCLASS()
class ULevel :
    public UObject
{
public:
	GENERATED_BODY()
	ULevel() = default;
	ULevel(UWorld* OwingWorld);
	ULevel(const TArray<AActor*>& Actors, UWorld* OwingWorld);
	~ULevel();

	void AddActor(AActor* Actor);
	void RemoveActor(AActor* Actor);
	void Clear();

	TArray<AActor*> GetActors() const;
	UWorld* GetWorld() const { return OwingWorld.Get(); }
	void SetWorld(UWorld* World) { OwingWorld = World; }

	void BeginPlay();
	void EndPlay();
	void RouteLevelDestroyed();
	void Tick(float DeltaTime);

    void AddReferencedObjects(FReferenceCollector& Collector) override;
    void BeginDestroy() override;
    
private:
	FName LevelName;

	// Runtime actor ownership. World/Level persistence is handled by SceneSaveManager, so this is GC-visible but not Save-serialized.
	UPROPERTY(Transient, Instanced, Category="Level")
	TArray<TObjectPtr<AActor>> Actors;

	// Non-owning back-reference to the world that owns this level.
	TWeakObjectPtr<UWorld> OwingWorld;
	bool bLevelDestroyRouted = false;
};

