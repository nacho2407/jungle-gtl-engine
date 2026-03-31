#pragma once

#include "Widget.h"
#include <functional>

class SButton : public SWidget
{
public:
	~SButton() override;

	FString Text;
	float FontSize = 12.0f;
	bool bEnabled = true;

	uint32 BackgroundColor = 0xFF3A3A3A;
	uint32 BorderColor = 0xFF6A6A6A;
	uint32 TextColor = 0xFFFFFFFF;
	uint32 DisabledBackgroundColor = 0xFF2E2E2E;
	uint32 DisabledTextColor = 0xFF9A9A9A;

	std::function<void()> OnClicked;

	void OnPaint(SWidget& Painter) override;
	bool OnMouseDown(int32 X, int32 Y) override;

private:
	FDynamicMesh* CachedTextMesh = nullptr;
	FString CachedText;
};

