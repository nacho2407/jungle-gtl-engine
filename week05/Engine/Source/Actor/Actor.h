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

	/** 자신이 속한 씬을 반환한다. */
	UScene* GetScene() const;
	/** 씬 등록 시 호출되며, 이 액터의 소속 씬을 갱신한다. */
	void SetScene(UScene* InScene);
	/** 소속 씬의 Outer를 따라가 현재 월드를 찾아 반환한다. */
	UWorld* GetWorld() const;
	// ULevel* GetLevel() const { return Level;
	// void SetLevel(ULevel* InLevel) { Level = InLevel; }

	/** 트랜스폼 기준이 되는 루트 씬 컴포넌트를 반환한다. */
	USceneComponent* GetRootComponent() const;
	/** 루트 컴포넌트를 지정하고, 필요하면 Owner도 함께 연결한다. */
	void SetRootComponent(USceneComponent* InRootComponent);

	/** 액터가 소유한 모든 컴포넌트 배열을 반환한다. */
	const TSet<UActorComponent*>& GetComponents() const;
	/** 액터에 새 컴포넌트를 소유권과 함께 등록한다. */
	void AddOwnedComponent(UActorComponent* InComponent);
	/** 액터에서 컴포넌트를 분리하고 소유 관계를 끊는다. */
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

	/** 스폰 직후 호출되며, 기본 컴포넌트 등록과 바운드 갱신 같은 초기 준비를 담당한다. */
	virtual void PostSpawnInitialize();
	/** 액터와 하위 컴포넌트에 BeginPlay를 전파한다. */
	virtual void BeginPlay();
	/** 활성화된 컴포넌트들을 프레임마다 진행시킨다. */
	virtual void Tick(float DeltaTime);
	/** 파괴 직전 별도 종료 처리 훅이다. */
	virtual void EndPlay();
	/** 액터를 삭제 예정 상태로 전환하고 모든 컴포넌트도 함께 정리 대상으로 표시한다. */
	virtual void Destroy();
	/** 액터와 소유 컴포넌트들의 직렬화/역직렬화를 수행한다. */
	virtual void Serialize(FArchive& Ar);

	bool HasBegunPlay() const { return bActorBegunPlay; }
	bool IsPendingDestroy() const { return bPendingDestroy; }
	bool CanTick() const { return bCanEverTick && bTickEnabled; }
	void SetActorTickEnabled(bool bEnabled) { bTickEnabled = bEnabled; }
	/** 루트 컴포넌트 기준 액터 위치를 반환한다. */
	const FVector& GetActorLocation() const;
	/** 루트 컴포넌트의 상대 위치를 변경해 액터 위치를 이동한다. */
	void SetActorLocation(const FVector& InLocation);

	bool IsVisible() const { return bVisible; }
	void SetVisible(bool bInVisible) { bVisible = bInVisible; }

protected:
	TObjectPtr<UScene> Scene;
	//ULevel* Level = nullptr;

	USceneComponent* RootComponent = nullptr;
	TSet<UActorComponent*> OwnedComponents;

	bool bCanEverTick = true;
	bool bTickEnabled = true;
	bool bActorBegunPlay = false;
	bool bPendingDestroy = false;
	bool bVisible = true;
};
