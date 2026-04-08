#pragma once
#include "Object/Object.h"
#include <memory>

class AActor;
class UWorld;
class FSpatialPartition;

class ULevel :
    public UObject
{
public:
	DECLARE_CLASS(ULevel, UObject)

	ULevel() = default;
	ULevel(UWorld* OwingWorld);
	ULevel(const TArray<AActor*>& Actors, UWorld* OwingWorld);
	~ULevel();

	void AddActor(AActor* Actor);
	void RemoveActor(AActor* Actor);
	void Clear();

	const TArray<AActor*>& GetActors() const { return Actors; }
	UWorld* GetWorld() const { return OwingWorld; }
	void SetWorld(UWorld* World) { OwingWorld = World;}

	void BeginPlay();
	void EndPlay();
	void Tick(float DeltaTime);
private:
	FName LevelName;
	TArray<AActor*> Actors;
	UWorld* OwingWorld = nullptr;
};

