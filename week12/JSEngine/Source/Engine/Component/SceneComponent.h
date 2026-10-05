#pragma once

#include "Engine/Geometry/Transform.h"
#include "Component/ActorComponent.h"
#include "Math/Utils.h"
#include "Object/FName.h"

class AActor;

UCLASS(SpawnableComponent, DisplayName = "Scene Component", Category = "Basic")
class USceneComponent : public UActorComponent
{
public:
	GENERATED_BODY(USceneComponent, UActorComponent)

	USceneComponent();
	~USceneComponent() override;

	virtual void PostDuplicate(UObject* Original) override;

	virtual void Serialize(FArchive& Ar) override;

	// Parent Relation Manager
	void AttachToComponent(USceneComponent* InParent, const FName& InSocketName = FName::None);
	void SetParent(USceneComponent* NewParent);
	USceneComponent* GetParent() const { return ParentComponent; }
	const FName& GetAttachSocketName() const { return AttachSocketName; }
	void AddChild(USceneComponent* NewChild);
	void RemoveChild(USceneComponent* Child);
	bool ContainsChild(const USceneComponent* Child) const;
	const TArray<USceneComponent*>& GetChildren() const { return ChildComponents; }

	// Socket API — default는 socket 없음. SkinnedMeshComponent 등에서 override.
	virtual bool       HasSocket(const FName& SocketName) const { (void)SocketName; return false; }
	virtual FTransform GetSocketTransform(const FName& SocketName) const { (void)SocketName; return GetWorldTransform(); }

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

	// 부모의 socket 이름. FName::None이면 일반 parent-child attach.
	UPROPERTY(DisplayName = "Attach Socket")
	FName AttachSocketName;

	mutable FMatrix CachedWorldMatrix{};
	mutable FTransform CachedWorldTransform{};
	mutable bool bTransformDirty = true;

	UPROPERTY(DisplayName = "Location", Speed = 0.1f, Animatable, LuaReadWrite, LuaName = Location)
	FVector RelativeLocation = FVector::ZeroVector;

	// 에디터 표시 및 직렬화용 Euler 값입니다. 로드/수정 후 RelativeRotationQuat를 재계산합니다.
	UPROPERTY(DisplayName = "Rotation", Speed = 0.1f, Animatable, LuaReadWrite, LuaName = Rotation)
	FVector RelativeRotation = FVector::ZeroVector;

	FQuat RelativeRotationQuat = FQuat::Identity; // 런타임 쿼터니언 캐시입니다.

	UPROPERTY(DisplayName = "Scale", Speed = 0.1f, Animatable, LuaReadWrite, LuaName = Scale)
	FVector RelativeScale3D = FVector(1.0f, 1.0f, 1.0f);
};
