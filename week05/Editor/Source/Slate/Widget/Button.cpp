#include "Button.h"

SButton::~SButton()
{
	delete CachedTextMesh;
}

void SButton::OnPaint(SWidget& Painter)
{
	const uint32 BgColor = bEnabled ? BackgroundColor : DisabledBackgroundColor;
	const uint32 LabelColor = bEnabled ? TextColor : DisabledTextColor;

	Painter.DrawRectFilled(Rect, BgColor);
	Painter.DrawRect(Rect, BorderColor);

	if (CachedText != Text || CachedLetterSpacing != LetterSpacing)
	{
		CachedText = Text;
		CachedLetterSpacing = LetterSpacing;
		delete CachedTextMesh;
		CachedTextMesh = nullptr;
	}

	const FVector2 TextSize = Painter.MeasureText(CachedText.c_str(), FontSize, LetterSpacing, CachedTextMesh);
	const int32 RoundedWidth = static_cast<int32>(TextSize.X + 0.5f);
	const int32 RoundedHeight = static_cast<int32>(TextSize.Y + 0.5f);

	int32 TextX = Rect.X;
	switch (TextHAlign)
	{
	case ETextHAlign::Left:   TextX = Rect.X; break;
	case ETextHAlign::Center: TextX = Rect.X + (Rect.Width - RoundedWidth) / 2; break;
	case ETextHAlign::Right:  TextX = Rect.X + Rect.Width - RoundedWidth; break;
	}

	int32 TextY = Rect.Y;
	switch (TextVAlign)
	{
	case ETextVAlign::Top:    TextY = Rect.Y; break;
	case ETextVAlign::Center: TextY = Rect.Y + (Rect.Height - RoundedHeight) / 2; break;
	case ETextVAlign::Bottom: TextY = Rect.Y + Rect.Height - RoundedHeight; break;
	}

	Painter.DrawText({ TextX, TextY }, CachedText.c_str(), LabelColor, FontSize, LetterSpacing, CachedTextMesh);

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
