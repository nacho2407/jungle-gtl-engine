#pragma once

#include "Math/Transform.h"
#include "Component/ActorComponent.h"
#include "Math/Utils.h"

class AActor;

class USceneComponent : public UActorComponent
{
public:
	DECLARE_CLASS(USceneComponent, UActorComponent)

	USceneComponent();
	~USceneComponent();

	// Parent Relation Manager
	void SetParent(USceneComponent* NewParent);
	USceneComponent* GetParent() { return ParentComponent; }
	void AddChild(USceneComponent* NewChild);
	void RemoveChild(USceneComponent* Child);
	bool ContainsChild(const USceneComponent* Child) const;
	const TArray<USceneComponent*>& GetChildren() const { return ChildComponents; }

	virtual void UpdateWorldMatrix();
	void AddWorldOffset(const FVector& WorldDelta);
	virtual void SetRelativeLocation(const FVector NewLocation);
	virtual void SetRelativeRotation(const FVector NewRotation);
	void SetRelativeScale(const FVector NewScale);
	void MarkTransformDirty();
	const FMatrix& GetWorldMatrix();
	void SetWorldLocation(FVector NewWorldLocation);
	FVector GetWorldLocation();
	FVector GetRelativeRotation() { return RelativeRotation; }
	FVector GetRelativeScale() { return RelativeScale3D; }
	FVector GetForwardVector();
	FVector GetUpVector();
	FVector GetRightVector();

	FMatrix GetRelativeMatrixTemp() const;

	// Make sure we are not facing gimbal lock
	void Move(const FVector& delta);
	void MoveLocal(const FVector& delta);
	void Rotate(float dx, float dy);

protected:
	USceneComponent* ParentComponent = nullptr;
	TArray<USceneComponent*> ChildComponents;

	FMatrix CachedWorldMatrix{};

	bool bTransformDirty = true;

public:
	FVector RelativeLocation{};
	FVector RelativeRotation{};
	FVector RelativeScale3D{ 1.0f, 1.0f ,1.0f };
};

