#pragma once

#include "Core/CoreTypes.h"

class FViewport;

// UE의 FViewportClient 대응 — 뷰포트 입력/드로잉 인터페이스
class FViewportClient
{
public:
	FViewportClient() = default;
	virtual ~FViewportClient() = default;

	virtual void Draw(FViewport* Viewport, float DeltaTime) {}
	virtual bool InputKey(int32 Key, bool bPressed) { return false; }
	virtual bool InputAxis(float DeltaX, float DeltaY) { return false; }
};
