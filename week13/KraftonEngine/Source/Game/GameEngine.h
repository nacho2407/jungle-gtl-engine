#pragma once

#include "Engine/Runtime/Engine.h"

class UGameEngine : public UEngine
{
public:
	DECLARE_CLASS(UGameEngine, UEngine)

	UGameEngine() = default;
	~UGameEngine() override = default;

	void Init(FWindowsWindow* InWindow) override;
	void Shutdown() override;
	void Tick(float DeltaTime) override;
	void OnWindowResized(uint32 Width, uint32 Height) override;

	FViewport* GetStandaloneViewport() const { return StandaloneViewport; }

	// 다음 frame Tick 끝에서 active world 를 destroy 하고 InScenePath 의 scene 으로 교체.
	// 호출은 Lua / GameMode 어디서든 안전 — 실제 destroy/load 는 World->Tick 바깥에서 일어나
	// 호출 stack 위의 액터/컴포넌트가 destroy 되어 use-after-free 가 나지 않는다.
	// "Go To Intro" / 매치 재시작 등 동적 상태 전체 리셋이 필요한 경우 사용.
	void RequestTransitionToScene(const FString& InScenePath);

private:
	void LoadStartLevel();
	bool LoadSceneFromPath(const FString& FilePath);

	// "Map" 같은 이름이나 Scene/.Scene 풀 경로 양쪽 다 받아 풀 파일 경로로 정규화.
	FString ResolveSceneFilePath(const FString& InNameOrPath) const;

	// UGameEngine::Tick 끝에서 호출 — 펜딩 요청이 있으면 이 시점에 destroy + load + BeginPlay 실행.
	void ProcessPendingTransition();

private:
	FViewport* StandaloneViewport = nullptr;

	bool bPendingSceneTransition = false;
	FString PendingScenePath;
};
