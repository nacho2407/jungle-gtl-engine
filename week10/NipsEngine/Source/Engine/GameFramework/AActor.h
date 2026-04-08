#pragma once
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"
#include "Engine/GameFramework/WorldContext.h"

#include <type_traits>

class UWorld;
class UPrimitiveComponent;

class AActor : public UObject {
public:
	DECLARE_CLASS(AActor, UObject)
	AActor() = default;
	~AActor() override;

	virtual AActor* Duplicate();
	virtual AActor* DuplicateSubObjects();

    virtual void InitDefaultComponents() {}

	// 컴포넌트 생성 + Owner 설정 + 등록만 수행. Attach는 별도로 호출할 것.
	template<typename T>
	T* AddComponent() {
		static_assert(std::is_base_of_v<UActorComponent, T>,
			"AddComponent<T>: T must derive from UActorComponent");

		T* Comp = UObjectManager::Get().CreateObject<T>();

		bPrimitiveCacheDirty = true;

		Comp->SetOwner(this);
		OwnedComponents.push_back(Comp);
		return Comp;
	}

	// Tick 관련
	virtual void BeginPlay();
	virtual void Tick(float DeltaTime);
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason);

	bool IsActive() const { return bIsActive ; }
	void SetActive(bool bEnabled) { bIsActive = bEnabled; }

	bool ShouldTickInEditor() const { return bTickInEditor; }
	void SetTickInEditor(bool bEnabled)  { bTickInEditor = bEnabled; }

	// FTypeInfo 기반 런타임 컴포넌트 생성
	UActorComponent* AddComponentByClass(const FTypeInfo* Class);
	void RemoveComponent(UActorComponent* Component);
	void RegisterComponent(UActorComponent* Comp);

	void SetRootComponent(USceneComponent* Comp);
	USceneComponent* GetRootComponent() const { return RootComponent; }

	const TArray<UActorComponent*>& GetComponents() const { return OwnedComponents; }

	// Transform — Location
	FVector GetActorLocation() const;
	void SetActorLocation(const FVector& Location);
	void AddActorWorldOffset(const FVector& Delta)
	{
		if (RootComponent) RootComponent->AddWorldOffset(Delta);
	}

	// Transform — Rotation
	FVector GetActorRotation() const
	{
		return RootComponent ? RootComponent->GetRelativeRotation() : FVector(0, 0, 0);
	}
	void SetActorRotation(const FVector& NewRotation)
	{
		if (RootComponent) RootComponent->SetRelativeRotation(NewRotation);
	}

	// Transform — Scale
	FVector GetActorScale() const
	{
		return RootComponent ? RootComponent->GetRelativeScale() : FVector(1, 1, 1);
	}
	void SetActorScale(const FVector& NewScale)
	{
		if (RootComponent) RootComponent->SetRelativeScale(NewScale);
	}

	// Direction
	FVector GetActorForward() const
	{
		if (RootComponent)
			return RootComponent->GetForwardVector();
		return FVector(0, 0, 1);
	}

	void SetWorld(UWorld* World) { OwningWorld = World; }
	UWorld* GetWorld() const { return OwningWorld; }

	bool IsVisible() const { return bVisible; }
	void SetVisible(bool Visible) { bVisible = Visible; }

	const TArray<UPrimitiveComponent*>& GetPrimitiveComponents() const;

protected:
	USceneComponent* RootComponent = nullptr;
	UWorld* OwningWorld = nullptr;

	FVector PendingActorLocation = FVector(0, 0, 0);

	bool bVisible = true;
	bool bIsActive = true;
    bool bTickInEditor = false;

	TArray<UActorComponent*> OwnedComponents;

	// 렌더링용 캐시
	mutable TArray<UPrimitiveComponent*> PrimitiveCache;
	mutable bool bPrimitiveCacheDirty = true;
};