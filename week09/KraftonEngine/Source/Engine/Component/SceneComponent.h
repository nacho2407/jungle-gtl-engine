#pragma once

#include "Math/Transform.h"
#include "Component/ActorComponent.h"
#include "Math/MathUtils.h"

class AActor;

class USceneComponent : public UActorComponent
{
public:
	DECLARE_CLASS(USceneComponent, UActorComponent)

	USceneComponent();
	~USceneComponent();

	// Parent Relation Manager
	void AttachToComponent(USceneComponent* InParent);
	void SetParent(USceneComponent* NewParent);
	USceneComponent* GetParent() const { return ParentComponent; }
	void AddChild(USceneComponent* NewChild);
	void RemoveChild(USceneComponent* Child);
	bool ContainsChild(const USceneComponent* Child) const;
	const TArray<USceneComponent*>& GetChildren() const { return ChildComponents; }

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;

	virtual void UpdateWorldMatrix() const;
	virtual void AddWorldOffset(const FVector& WorldDelta);
	virtual void SetRelativeLocation(const FVector& NewLocation);
	virtual void SetRelativeRotation(const FVector& NewRotation);
	virtual void SetRelativeScale(const FVector& NewScale);
	void MarkTransformDirty();
	const FMatrix& GetWorldMatrix() const;
	void SetWorldLocation(FVector NewWorldLocation);
	FVector GetWorldLocation() const;
	FVector GetWorldScale() const;
	FVector GetRelativeLocation() const { return RelativeLocation; }
	FVector GetRelativeRotation() const { return RelativeRotation; }
	FVector GetRelativeScale() const { return RelativeScale3D; }
	FVector GetForwardVector() const;
	FVector GetUpVector() const;
	FVector GetRightVector() const;

	FMatrix GetRelativeMatrix() const;

	void Move(const FVector& Delta);
	void MoveLocal(const FVector& Delta);
	void Rotate(float DeltaYaw, float DeltaPitch);

protected:
	USceneComponent* ParentComponent = nullptr;
	TArray<USceneComponent*> ChildComponents;

	mutable FMatrix CachedWorldMatrix{};

	mutable bool bTransformDirty = true;

	FVector RelativeLocation{};
	FVector RelativeRotation{};
	FVector RelativeScale3D{ 1.0f, 1.0f ,1.0f };
};

