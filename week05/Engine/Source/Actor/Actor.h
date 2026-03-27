#pragma once
#include "Object/Object.h"
#include "World/World.h"
#include "Component/ActorComponent.h"

class UActorComponent;
class USceneComponent;
class UScene;

class FArchive;
class ENGINE_API AActor : public UObject
{
public:
	DECLARE_RTTI(AActor, UObject)
	~AActor() override = default;

	UScene* GetScene() const;
	void SetScene(UScene* InScene);
	UWorld* GetWorld() const;
	// ULevel* GetLevel() const { return Level;
	// void SetLevel(ULevel* InLevel) { Level = InLevel; }

	USceneComponent* GetRootComponent() const;
	void SetRootComponent(USceneComponent* InRootComponent);

	const TArray<UActorComponent*>& GetComponents() const;
	void AddOwnedComponent(UActorComponent* InComponent);
	void RemoveOwnedComponent(UActorComponent* InComponent);

	template <typename T>
	T* GetComponentByClass() const
	{
		for (UActorComponent* Component : OwnedComponents)
		{
			if (Component && Component->IsA(T::StaticClass()))
			{
				return static_cast<T*>(Component);
			}
		}
		return nullptr;
	}

	void Test() { int a = 5; }
	
	virtual void PostSpawnInitialize();
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);
	virtual void EndPlay();
	virtual void Destroy();
	//Add direct Serializer
	virtual void Serialize(FArchive& Ar);

	bool HasBegunPlay() const { return bActorBegunPlay; }
	bool IsPendingDestroy() const { return bPendingDestroy; }
	bool CanTick() const { return bCanEverTick && bTickEnabled; }
	void SetActorTickEnabled(bool bEnabled) { bTickEnabled = bEnabled; }
	const FVector& GetActorLocation() const;
	void SetActorLocation(const FVector& InLocation);


	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }
protected:
	TObjectPtr<UScene> Scene;
	//ULevel* Level = nullptr;

	USceneComponent* RootComponent = nullptr;
	TArray<UActorComponent*> OwnedComponents;

	bool bCanEverTick = true;
	bool bTickEnabled = true;
	bool bActorBegunPlay = false;
	bool bPendingDestroy = false;
	bool bVisible = true;
};

