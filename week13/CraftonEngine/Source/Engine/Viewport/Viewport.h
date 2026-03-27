#pragma once

#include "Core/CoreTypes.h"

class FViewportClient;

// UE의 FViewport 대응 — 렌더 타깃 영역 + 소유 ViewportClient 참조
class FViewport
{
public:
	FViewport() = default;
	virtual ~FViewport() = default;

	void SetClient(FViewportClient* InClient) { ViewportClient = InClient; }
	FViewportClient* GetClient() const { return ViewportClient; }

	void SetSize(uint32 InWidth, uint32 InHeight) { Width = InWidth; Height = InHeight; }
	uint32 GetWidth() const { return Width; }
	uint32 GetHeight() const { return Height; }

private:
	FViewportClient* ViewportClient = nullptr;
	uint32 Width = 0;
	uint32 Height = 0;
};
