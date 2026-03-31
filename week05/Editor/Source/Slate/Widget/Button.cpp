#include "Button.h"

SButton::~SButton()
{
	delete CachedTextMesh;
}

void SButton::OnPaint(SWidget& Painter)
{
	const uint32 BgColor = bEnabled ? BackgroundColor : DisabledBackgroundColor;
	const uint32 LabelColor = bEnabled ? TextColor : DisabledTextColor;
	const int32 TextY = Rect.Y + (Rect.Height - static_cast<int32>(FontSize)) / 2;

	Painter.DrawRectFilled(Rect, BgColor);
	Painter.DrawRect(Rect, BorderColor);

	if (CachedText != Text)
	{
		CachedText = Text;
		delete CachedTextMesh;
		CachedTextMesh = nullptr;
	}

	Painter.DrawText({ Rect.X + 8, TextY }, CachedText.c_str(), LabelColor, FontSize, CachedTextMesh);
}

bool SButton::OnMouseDown(int32 X, int32 Y)
{
	if (!IsHover({ X, Y }))
	{
		return false;
	}

	if (bEnabled && OnClicked)
	{
		OnClicked();
	}

	return true;
}
