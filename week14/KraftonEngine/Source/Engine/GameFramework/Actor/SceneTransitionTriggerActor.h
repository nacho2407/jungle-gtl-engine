#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/SceneTransitionTriggerActor.generated.h"
class UBoxComponent;
class APawn;

/**
 * @brief 박스 안으로 possessed pawn이 들어오면 일정 지연 뒤 다른 Scene으로 전환하는 트리거
 *
 * @details PhysX overlap 대신 매 Tick 마다 BoxComponent의 local space에서 OBB 포함
 *          여부를 직접 검사한다. Level1 -> Level2, Level2 -> Level3 등 게이트 전이용.
 */
UCLASS()
class ASceneTransitionTriggerActor : public AActor
{
public:
	GENERATED_BODY()
	ASceneTransitionTriggerActor() = default;

	void InitDefaultComponents();
	void PostDuplicate() override;
	void BeginPlay() override;
	void Tick(float DeltaTime) override;

	UBoxComponent* GetBoxComponent() const { return BoxComponent; }

private:
	bool IsPawnInsideBox(const APawn* Pawn) const;
	void FireTransition();

	UBoxComponent* BoxComponent = nullptr;

	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Target Scene")
	FString TargetScene = "";  // "FL_Level2" / "Content/Scene/FL_Level2.Scene" 모두 허용

	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Transition Delay", Min=0.0f, Speed=0.1f)
	float TransitionDelay = 3.0f;

	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Trigger Once")
	bool bTriggerOnce = true;

	bool bCountingDown = false;
	bool bConsumed = false;
	float ElapsedSinceEnter = 0.0f;
};
