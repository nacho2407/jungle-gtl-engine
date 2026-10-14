#pragma once

#include "Component/ActorComponent.h"
#include "Component/SceneComponent.h"

//TODO : 해당 컴포넌트 베이스 역할을 하고 고유의 기능은 없기에 오브젝트에 부여할 수 없도록 바꿔야 합니다!

/**
 * 런타임(PIE, Game mode) 동안
 * USceneComponent를 움직이는 로직들의 베이스 클래스.
 * 실제 이동 로직은 자식 클래스에서 담당합니다.
 */
#include "Source/Engine/Component/Movement/MovementComponent.generated.h"

UCLASS()
class UMovementComponent : public UActorComponent
{
public:
	GENERATED_BODY()

	UMovementComponent() = default;
	~UMovementComponent() override = default;

	void BeginPlay() override;
	void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
	void PostEditProperty(const char* PropertyName) override;

	void SetUpdatedComponent(USceneComponent* NewUpdatedComponent);
	USceneComponent* GetUpdatedComponent() const;
	bool HasValidUpdatedComponent() const { return GetUpdatedComponent() != nullptr; }
	FString GetUpdatedComponentDisplayName() const;
	TArray<USceneComponent*> GetOwnerSceneComponents() const;
	bool ResolveUpdatedComponent();
	void ClearUpdatedComponentIfMatches(const USceneComponent* RemovedComponent);

protected:
	void TryAutoRegisterUpdatedComponent();

	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Updated Component")
	USceneComponent* UpdatedComponent = nullptr; // 움직일 대상
	UPROPERTY(Edit, Save, Category="Movement", DisplayName="Auto Register Updated")
	bool bAutoRegisterUpdatedComponent = true;
};
