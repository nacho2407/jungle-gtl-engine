#pragma once

#include "Math/Transform.h"
#include "Math/Rotator.h"
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
	void AttachToComponent(USceneComponent* InParent);
	void SetParent(USceneComponent* NewParent);
	USceneComponent* GetParent() const { return ParentComponent; }
	void AddChild(USceneComponent* NewChild);
	void RemoveChild(USceneComponent* Child);
	bool ContainsChild(const USceneComponent* Child) const;
	const TArray<USceneComponent*>& GetChildren() const { return ChildComponents; }

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	virtual void UpdateWorldMatrix() const;
	virtual void AddWorldOffset(const FVector& WorldDelta);
	virtual void SetRelativeLocation(const FVector& NewLocation);
	virtual void SetRelativeRotation(const FRotator& NewRotation);
	virtual void SetRelativeRotation(const FQuat& NewRotation);
	void SetRelativeRotation(const FVector& EulerRotation);	// FVector 호환
	virtual void SetRelativeScale(const FVector& NewScale);
	void MarkTransformDirty();
	void ApplyCachedEditRotator();
	const FMatrix& GetWorldMatrix() const;
	void SetWorldLocation(FVector NewWorldLocation);
	FVector GetWorldLocation() const;
	FVector GetWorldScale() const;
	const FTransform& GetRelativeTransform() const { return RelativeTransform; }
	FVector GetRelativeLocation() const { return RelativeTransform.Location; }
	FRotator GetRelativeRotation() const { return RelativeTransform.GetRotator(); }
	const FQuat& GetRelativeQuat() const { return RelativeTransform.Rotation; }
	FVector GetRelativeScale() const { return RelativeTransform.Scale; }
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

	FTransform RelativeTransform;
	mutable FRotator CachedEditRotator;	// 에디터 프로퍼티 바인딩용
};

