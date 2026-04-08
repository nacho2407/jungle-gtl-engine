#pragma once

#include "Component/ActorComponent.h"

class USceneComponent;

/**
 * 런타임(PIE, Game mode) 동안
 * USceneComponent를 움직이는 로직들의 베이스 클래스.
 * 실제 이동 로직은 자식 클래스에서 담당합니다.
 * 즉 해당 컴포넌트 자체는 별 쓸모가 없기에 에디터에서 오브젝트에 부여할 수 없어야 합니다!
 */
class UMovementComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UMovementComponent, UActorComponent)

	UMovementComponent() = default;
	~UMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void Serialize(FArchive& Ar) override;

	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	USceneComponent* GetUpdatedComponent() const { return UpdatedComponent; }
	bool HasValidUpdatedComponent() const { return UpdatedComponent != nullptr; }

protected:
	void TryAutoRegisterUpdatedComponent();

	USceneComponent* UpdatedComponent = nullptr; // 움직일 대상
	bool bAutoRegisterUpdatedComponent = true;
};
