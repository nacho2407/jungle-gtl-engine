#pragma once

#include "Engine/Geometry/Transform.h"
#include "Component/ActorComponent.h"
#include "Math/Utils.h"

class AActor;

class USceneComponent : public UActorComponent
{
public:
	DECLARE_CLASS(USceneComponent, UActorComponent)

	USceneComponent();
	~USceneComponent() override;

	virtual void PostDuplicate(UObject* Original) override;

	virtual void Serialize(FArchive& Ar) override;

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
	virtual void SetRelativeRotation(const FVector& NewRotation);
	virtual void SetRelativeScale(const FVector& NewScale);

	void MarkTransformDirty();

	FTransform GetRelativeTransform() const;
	FTransform GetWorldTransform() const;
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

	// 기존 시그니처 유지
	// DeltaYaw  : world up(Z) 기준 yaw 입력값(도)
	// DeltaPitch: local right(Y) 기준 pitch 입력값(도)
	void Rotate(float DeltaYaw, float DeltaPitch);

	FQuat GetRelativeQuat() const;
	void SetRelativeRotationQuat(const FQuat& NewRotationQuat);

protected:
    /** @brief Hook fired when this component becomes transform-dirty. */
    virtual void OnTransformDirty() {}

	FRotator GetRelativeRotator() const;
	void SetRelativeRotationRotator(const FRotator& NewRotation);

	// yaw는 world-up, pitch는 local-right 기준으로 적용
	void AddRelativeYaw(float DeltaYawDegrees);
	void AddRelativePitch(float DeltaPitchDegrees);

protected:
	USceneComponent* ParentComponent = nullptr;
	TArray<USceneComponent*> ChildComponents;

	mutable FMatrix CachedWorldMatrix{};
	mutable FTransform CachedWorldTransform{};
	mutable bool bTransformDirty = true;

	FVector RelativeLocation{};
	// 에디터 표시 및 직렬화용 오일러 캐시 (Roll, Pitch, Yaw 도 단위)
	// 회전 연산의 권위 있는 소스는 RelativeRotationQuat 입니다.
	FVector RelativeRotation{};
	// 권위 있는 회전 저장소 — 짐벌 락 없는 쿼터니언으로 유지합니다.
	FQuat RelativeRotationQuat = FQuat::Identity;
	FVector RelativeScale3D{ 1.0f, 1.0f, 1.0f };
};
