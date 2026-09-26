#pragma once
#include "CoreMinimal.h"
#include "ActorComponent.h"

class ENGINE_API USceneComponent : public UActorComponent
{
public:
	DECLARE_RTTI(USceneComponent, UActorComponent)

	/** 부모 기준 상대 트랜스폼 전체를 반환한다. */
	const FTransform& GetRelativeTransform() const { return RelativeTransform; }
	/** 상대 위치/회전/스케일을 한 번에 바꾸고 월드 트랜스폼을 다시 계산하도록 표시한다. */
	void SetRelativeTransform(const FTransform& InTransform);

	/** 부모 기준 상대 위치만 반환한다. */
	const FVector& GetRelativeLocation() const { return RelativeTransform.GetTranslation(); }
	/** 상대 위치만 바꾸고 하위 트리 전체를 dirty 상태로 만든다. */
	void SetRelativeLocation(const FVector& InLocation);

	/** 현재 부모 씬 컴포넌트를 반환한다. */
	USceneComponent* GetAttachParent() const { return AttachParent; }
	/** 현재 자신에게 붙은 자식 씬 컴포넌트 목록을 반환한다. */
	const TArray<USceneComponent*>& GetAttachChildren() const { return AttachChildren; }

	/** 부모를 새로 연결하고, 이전 부모와의 연결은 자동으로 해제한다. */
	void AttachTo(USceneComponent* InParent);
	/** 현재 부모와의 연결만 끊고 월드 트랜스폼을 다시 계산하도록 만든다. */
	void DetachFromParent();
	/** 캐시된 월드 트랜스폼에서 위치 성분만 꺼내 반환한다. */
	FVector GetWorldLocation() const;
	/** 필요할 때만 계산되는 월드 트랜스폼 캐시를 반환한다. */
	const FMatrix& GetWorldTransform() const;

	/** 상대 위치/회전/스케일을 직렬화해 씬 저장 데이터와 연결한다. */
	void Serialize(FArchive& Ar) override;
	void DuplicateShallow(UObject* DuplicatedObject, FDuplicateContext& Context) const override;
	void FixupDuplicatedReferences(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;
	void PostDuplicate(UObject* DuplicatedObject, const FDuplicateContext& Context) const override;

protected:
	/** 자신과 자식들의 월드 트랜스폼 캐시를 무효화한다. */
	virtual void MarkTransformDirty();

private:
	/** 부모 트랜스폼까지 반영해 월드 변환 행렬 캐시를 다시 만든다. */
	void UpdateWorldTransform() const;

	FTransform RelativeTransform{};
	TObjectPtr<USceneComponent> AttachParent;
	TArray<USceneComponent*> AttachChildren;

	mutable FMatrix CachedWorldTransform;
	mutable bool bWorldTransformDirty = true;
};
