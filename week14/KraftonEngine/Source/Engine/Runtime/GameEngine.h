#pragma once

#include "Core/Types/EngineTypes.h"
#include "Engine/Runtime/Engine.h"


#include "Source/Engine/Runtime/GameEngine.generated.h"

UCLASS()
class UGameEngine : public UEngine
{
public:
	GENERATED_BODY()
	UGameEngine() = default;
	~UGameEngine() override = default;

	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	void OnWindowResized(uint32 Width, uint32 Height) override;

	FViewport* GetStandaloneViewport() const { return StandaloneViewport; }

	// 다음 frame Tick 끝에서 active world 를 destroy 하고 InScenePath 의 scene 으로 교체.
	// 호출은 Lua gameplay code 어디서든 안전 — 실제 destroy/load 는 World->Tick 바깥에서 일어나
	// 호출 stack 위의 액터/컴포넌트가 destroy 되어 use-after-free 가 나지 않는다.
	// "Go To Intro" / 매치 재시작 등 동적 상태 전체 리셋이 필요한 경우 사용.
	void RequestTransitionToScene(const FString& InScenePath) override;

	// 다음 scene transition 직후 새 PlayerCameraManager 의 fade 를 alpha 1 → 0 으로 시작하도록
	// 예약. 트리거가 fade-out 을 시작한 뒤 destroyed 되더라도 fade-in 정보가 살아남도록
	// 엔진 측 상태로 보관 — ProcessPendingTransition 이 새 scene BeginPlay 뒤에 적용한다.
	void SetPendingFadeIn(float Duration, FLinearColor Color = FLinearColor::Black());

private:
	void LoadStartLevel();
	bool LoadSceneFromPath(const FString& FilePath);

	// "Map" 같은 이름이나 Scene/.Scene 풀 경로 양쪽 다 받아 풀 파일 경로로 정규화.
	FString ResolveSceneFilePath(const FString& InNameOrPath) const;

	// UGameEngine::Tick 끝에서 호출 — 펜딩 요청이 있으면 이 시점에 destroy + load + BeginPlay 실행.
	void ProcessPendingTransition();

	// 새 scene 의 PlayerCameraManager 가 준비된 시점에 펜딩 fade-in 을 시작하고 플래그 클리어.
	void ApplyPendingFadeIn();

private:
	FViewport* StandaloneViewport = nullptr;

	bool bPendingSceneTransition = false;
	FString PendingScenePath;

	bool bHasPendingFadeIn = false;
	float PendingFadeInDuration = 0.0f;
	FLinearColor PendingFadeInColor = FLinearColor::Black();
};
