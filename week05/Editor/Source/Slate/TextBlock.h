#pragma once

#include "Widget.h"
#include "Renderer/MeshData.h"

#ifdef DrawText
#undef DrawText
#endif

class STextBlock : public SWidget
{
public:
	~STextBlock() { delete CachedMesh; }
	FString Text;
	uint32 Color = 0xFFFFFFFF;
	float FontSize = 48.0f;
	
	void SetText(const FString& InText);
	FVector2 ComputeDesiredSize() const override { return { (float)Text.size() * FontSize * 0.6f, FontSize }; }
	void OnPaint(SWidget& Painter) override { Painter.DrawText({ Rect.X, Rect.Y }, Text.c_str(), Color, FontSize, CachedMesh); }

private:
	FDynamicMesh* CachedMesh = nullptr;
};