#pragma once

#include "Math/Transform.h"
#include "Math/Rotator.h"
#include "Component/ActorComponent.h"
#include "Math/MathUtils.h"

class AActor;

#include "Source/Engine/Component/SceneComponent.generated.h"

UCLASS()
class USceneComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	USceneComponent();
	~USceneComponent();

	// Parent Relation Manager
	UFUNCTION(Callable, Category="Scene|Hierarchy")
	void AttachToComponent(USceneComponent* InParent);
	UFUNCTION(Callable, Category="Scene|Hierarchy")
	void SetParent(USceneComponent* NewParent);
	UFUNCTION(Pure, Category="Scene|Hierarchy")
	USceneComponent* GetParent() const { return ParentComponent; }
	UFUNCTION(Callable, Category="Scene|Hierarchy")
	void AddChild(USceneComponent* NewChild);
	UFUNCTION(Callable, Category="Scene|Hierarchy")
	void RemoveChild(USceneComponent* Child);
	UFUNCTION(Pure, Category="Scene|Hierarchy")
	bool ContainsChild(const USceneComponent* Child) const;
	const TArray<USceneComponent*>& GetChildren() const { return ChildComponents; }

	void PreGetEditableProperties() override;
	void PostEditProperty(const char* PropertyName) override;

	void Serialize(FArchive& Ar) override;

	virtual void UpdateWorldMatrix() const;
	UFUNCTION(Callable, Category="Scene|Transform")
	virtual void AddWorldOffset(const FVector& WorldDelta);
	UFUNCTION(Callable, Category="Scene|Transform")
	virtual void SetRelativeLocation(const FVector& NewLocation);
	UFUNCTION(Callable, Category="Scene|Transform")
	virtual void SetRelativeRotation(const FRotator& NewRotation);
	virtual void SetRelativeRotation(const FQuat& NewRotation);
	void SetRelativeRotation(const FVector& EulerRotation);	// FVector 호환
	UFUNCTION(Callable, Category="Scene|Transform")
	virtual void SetRelativeScale(const FVector& NewScale);
	void SetRelativeTransform(const FTransform& NewTransform);
	UFUNCTION(Callable, Category="Scene|Transform")
	void SetAbsoluteScale(bool bInAbsoluteScale) { bAbsoluteScale = bInAbsoluteScale; MarkTransformDirty(); }
	UFUNCTION(Pure, Category="Scene|Transform")
	bool IsAbsoluteScale() const { return bAbsoluteScale; }

	// 누적 회전용 — Quat 합성으로 적용해 짐벌락/Euler 라운드트립 손실을 회피한다.
	// 매 프레임 누적이 필요한 코드는 GetRelativeRotation()→+delta→Set 패턴 대신 이걸 써야 한다.
	void AddLocalRotation(const FQuat& DeltaQuat);
	UFUNCTION(Callable, Category="Scene|Transform")
	void AddLocalRotation(const FRotator& DeltaRotator);
	void MarkTransformDirty();
	virtual void OnTransformDirty();
	void ApplyCachedEditRotator();
	FRotator& GetCachedEditRotator();	// 에디터 UI용 Euler 캐시 접근
	// Quat을 직접 세팅하면서 Euler 캐시도 함께 지정 (짐벌락 방지)
	void SetRelativeRotationWithEulerHint(const FQuat& NewQuat, const FRotator& EulerHint);
	const FMatrix& GetWorldMatrix() const;
	const FMatrix& GetWorldInverseMatrix() const;
	UFUNCTION(Callable, Category="Scene|Transform")
	void SetWorldLocation(FVector NewWorldLocation);
	UFUNCTION(Pure, Category="Scene|Transform")
	FVector GetWorldLocation() const;
	UFUNCTION(Pure, Category="Scene|Transform")
	FRotator GetWorldRotation() const;
	UFUNCTION(Pure, Category="Scene|Transform")
	FVector GetWorldScale() const;
	const FTransform& GetRelativeTransform() const { return RelativeTransform; }
	UFUNCTION(Pure, Category="Scene|Transform")
	FVector GetRelativeLocation() const { return RelativeTransform.Location; }
	UFUNCTION(Pure, Category="Scene|Transform")
	FRotator GetRelativeRotation() const { return RelativeTransform.GetRotator(); }
	const FQuat& GetRelativeQuat() const { return RelativeTransform.Rotation; }
	UFUNCTION(Pure, Category="Scene|Transform")
	FVector GetRelativeScale() const { return RelativeTransform.Scale; }
	UFUNCTION(Pure, Category="Scene|Transform")
	FVector GetForwardVector() const;
	UFUNCTION(Pure, Category="Scene|Transform")
	FVector GetUpVector() const;
	UFUNCTION(Pure, Category="Scene|Transform")
	FVector GetRightVector() const;

	FMatrix GetRelativeMatrix() const;

	UFUNCTION(Callable, Category="Scene|Transform")
	void Move(const FVector& Delta);
	UFUNCTION(Callable, Category="Scene|Transform")
	void MoveLocal(const FVector& Delta);
	UFUNCTION(Callable, Category="Scene|Transform")
	void Rotate(float DeltaYaw, float DeltaPitch);

    void AddReferencedObjects(FReferenceCollector& Collector) override;
    void BeginDestroy() override;

protected:
	USceneComponent* ParentComponent = nullptr;
	TArray<USceneComponent*> ChildComponents;

	bool bAbsoluteScale = false;

	mutable bool bTransformDirty = true;

	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Location", Member=RelativeTransform.Location, Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f);
	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Scale", Member=RelativeTransform.Scale, Type=Vec3, Min=0.0f, Max=0.0f, Speed=0.1f);
	FTransform RelativeTransform;
	UPROPERTY(Edit, Save, Category="Transform", DisplayName="Rotation", Type=Rotator, Min=0.0f, Max=0.0f, Speed=0.1f)
	mutable FRotator CachedEditRotator;	// 에디터 프로퍼티 바인딩용 (Euler 캐시)
	mutable bool bCachedEulerDirty = true;	// Quat가 외부에서 변경됐을 때만 Euler 재계산

	//world matrix caching
	mutable FMatrix CachedWorldMatrix{};
	//inverse world matrix caching
	mutable FMatrix CachedInverseWorldMatrix{};
	mutable bool bInverseWorldDirty = true;
};

