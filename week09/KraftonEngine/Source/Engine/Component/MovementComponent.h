#pragma once

#include "Component/ActorComponent.h"

class USceneComponent;

/**
 * 런타임(PIE, Game mode) 동안
 * USceneComponent를 움직이는 로직들의 베이스 클래스.
 * 실제 이동 로직은 자식 클래스에서 담당합니다.
 */
class UMovementComponent : public UActorComponent
{
public:
	DECLARE_CLASS(UMovementComponent, UActorComponent)

	UMovementComponent() = default;
	~UMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime) override;
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
