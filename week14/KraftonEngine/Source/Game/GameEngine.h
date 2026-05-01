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

private:
	void LoadStartLevel();
	bool LoadSceneFromPath(const FString& FilePath);

private:
	FViewport* StandaloneViewport = nullptr;
};
