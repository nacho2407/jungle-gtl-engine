#pragma once

#include "Object/Object.h"
#include "Viewport/ViewportClient.h"

class FViewport;

// UE의 UGameViewportClient 대응 — UObject + FViewportClient 다중상속
// 게임 런타임 뷰포트를 담당 (PIE / Standalone)
class UGameViewportClient : public UObject, public FViewportClient
{
public:
	DECLARE_CLASS(UGameViewportClient, UObject)

	UGameViewportClient() = default;
	~UGameViewportClient() override = default;

	// FViewportClient overrides
	void Draw(FViewport* Viewport, float DeltaTime) override {}
	bool InputKey(int32 Key, bool bPressed) override { return false; }

	// Viewport 소유
	void SetViewport(FViewport* InViewport) { Viewport = InViewport; }
	FViewport* GetViewport() const { return Viewport; }

private:
	FViewport* Viewport = nullptr;
};
