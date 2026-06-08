#pragma once

#include "GameFramework/AActor.h"

#include "Source/Engine/GameFramework/Actor/SceneTransitionTriggerActor.generated.h"
class UBoxComponent;
class APawn;

/**
 * @brief 박스 안으로 possessed pawn이 들어오면 fade-out과 loading screen 후 다른 Scene으로 전환하는 트리거
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
	void BeginFadeOut(class APlayerCameraManager* CamMgr);
	void ShowLoadingScreen();

	UBoxComponent* BoxComponent = nullptr;

	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Target Scene")
	FString TargetScene = "";  // "FL_Level2" / "Content/Scene/FL_Level2.Scene" 모두 허용

	// 이전 scene 직렬화 호환용 값. 현재 전환은 트리거 진입 즉시 fade-out 을 시작합니다.
	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Legacy Transition Delay", Min=0.0f, Speed=0.1f)
	float TransitionDelay = 3.0f;

	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Trigger Once")
	bool bTriggerOnce = true;

	// Fade-out 길이. 0 이면 fade-out 연출 없이 바로 loading screen 단계로 넘어갑니다.
	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Fade Out Duration", Min=0.0f, Speed=0.1f)
	float FadeOutDuration = 1.5f;

	// 새 scene 로드 직후 적용되는 fade-in 길이. 0 이면 fade-in 없이 바로 보임.
	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Fade In Duration", Min=0.0f, Speed=0.1f)
	float FadeInDuration = 1.0f;

	// fade-out 완료 후 scene load 전에 loading screen 을 최소로 보여줄 시간.
	UPROPERTY(Edit, Save, Category="SceneTransition", DisplayName="Loading Screen Duration", Min=0.0f, Speed=0.1f)
	float LoadingScreenDuration = 0.75f;

	bool bCountingDown = false;
	bool bConsumed = false;
	bool bFadeOutStarted = false;
	bool bLoadingScreenShown = false;
	float ElapsedSinceEnter = 0.0f;
};
