#pragma once
#include "Object/Object.h"
#include "Object/ObjectFactory.h"
#include "Component/SceneComponent.h"

class UWorld;
class UPrimitiveComponent;

class AActor : public UObject {
public:
	DECLARE_CLASS(AActor, UObject)
	AActor() = default;
	~AActor() override;

	virtual void BeginPlay() {}
	virtual void Tick(float DeltaTime);
	virtual void EndPlay() {}

	template<typename T>
	T* AddComponent() {
		T* component = UObjectManager::Get().CreateObject<T>();

		// Remeber to add this later
		component->SetOwner(this);

		// First component added becomes the root
		if (!RootComponent)
		{
			RootComponent = component;
			RootComponent->SetWorldLocation(PendingActorLocation);
		}
		else
		{
			// Attach to root by default
			component->SetParent(RootComponent);
		}

		Components.push_back(component);
		return component;
	}

	USceneComponent* AddComponent(USceneComponent* ExistingComp) {
		if (!ExistingComp) return nullptr;

		if (!RootComponent) {
			RootComponent = ExistingComp;
			RootComponent->SetWorldLocation(PendingActorLocation);
		}
		else {
			ExistingComp->SetParent(RootComponent);
		}

		RegisterComponentRecursive(ExistingComp);
		return ExistingComp;
	}

	void RemoveComponent(USceneComponent* Component) {
		if (!Component) return;

		auto it = std::find(Components.begin(),
			Components.end(), Component);
		if (it != Components.end())
			Components.erase(it);

		if (RootComponent == Component)
			RootComponent = Components.empty()
			? nullptr
			: Components[0];

		UObjectManager::Get().DestroyObject(Component);
	}

	void SetRootComponent(USceneComponent* Comp) {
		if (!Comp) return;
		RootComponent = Comp;
		Components.clear(); // rebuild from scratch
		RegisterComponentRecursive(Comp);
	}

	USceneComponent* GetRootComponent() const { return RootComponent; }
	const TArray<USceneComponent*>& GetComponents() const { return Components; }
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

	TArray<USceneComponent*> Components;

	// 렌더링용 캐시
	mutable TArray<UPrimitiveComponent*> PrimitiveCache;
	mutable bool bComponentsDirty = true;

private:
	void RegisterComponentRecursive(USceneComponent* Comp);
};