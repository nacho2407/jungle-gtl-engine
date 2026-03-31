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
	float LetterSpacing = 1.0f;
	ETextHAlign TextHAlign = ETextHAlign::Left;
	ETextVAlign TextVAlign = ETextVAlign::Center;
	
	void SetText(const FString& InText);
	FVector2 ComputeDesiredSize() const override { return { (float)Text.size() * FontSize * 0.6f * LetterSpacing, FontSize }; }
	void OnPaint(SWidget& Painter) override
	{
		if (CachedRenderedText != Text || CachedLetterSpacing != LetterSpacing)
		{
			CachedRenderedText = Text;
			CachedLetterSpacing = LetterSpacing;
			delete CachedMesh;
			CachedMesh = nullptr;
		}

		const FVector2 TextSize = Painter.MeasureText(Text.c_str(), FontSize, LetterSpacing, CachedMesh);
		const int32 TextWidth = static_cast<int32>(TextSize.X + 0.5f);
		const int32 TextHeight = static_cast<int32>(TextSize.Y + 0.5f);

		int32 TextX = Rect.X;
		switch (TextHAlign)
		{
		case ETextHAlign::Left:
			TextX = Rect.X;
			break;
		case ETextHAlign::Center:
			TextX = Rect.X + (Rect.Width - TextWidth) / 2;
			break;
		case ETextHAlign::Right:
			TextX = Rect.X + Rect.Width - TextWidth;
			break;
		}

		int32 TextY = Rect.Y;
		switch (TextVAlign)
		{
		case ETextVAlign::Top:
			TextY = Rect.Y;
			break;
		case ETextVAlign::Center:
			TextY = Rect.Y + (Rect.Height - TextHeight) / 2;
			break;
		case ETextVAlign::Bottom:
			TextY = Rect.Y + Rect.Height - TextHeight;
			break;
		}

		Painter.DrawText({ TextX, TextY }, Text.c_str(), Color, FontSize, LetterSpacing, CachedMesh);
	}

private:
	FDynamicMesh* CachedMesh = nullptr;
	FString CachedRenderedText;
	float CachedLetterSpacing = 1.0f;
};
