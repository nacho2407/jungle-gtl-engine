#include "Painter.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <utility>

namespace
{
	static FUIRect ToUIRect(const FRect& Rect)
	{
		FUIRect Out;
		Out.X = static_cast<float>(Rect.X);
		Out.Y = static_cast<float>(Rect.Y);
		Out.Width = static_cast<float>(Rect.Width);
		Out.Height = static_cast<float>(Rect.Height);
		return Out;
	}

	static float EstimateGlyphAdvanceUnit(const char* Ptr, int32& OutByteCount)
	{
		const unsigned char C = static_cast<unsigned char>(Ptr[0]);
		if (C < 0x80)
		{
			OutByteCount = 1;

			if (C == static_cast<unsigned char>(' '))
			{
				return 0.33f;
			}

			if (C == static_cast<unsigned char>('\t'))
			{
				return 1.30f;
			}

			if (std::isdigit(C))
			{
				return 0.58f;
			}

			if (std::ispunct(C))
			{
				return 0.48f;
			}

			return 0.62f;
		}

		if ((C & 0xE0) == 0xC0)
		{
			OutByteCount = 2;
			return 0.92f;
		}

		if ((C & 0xF0) == 0xE0)
		{
			OutByteCount = 3;
			return 1.00f;
		}

		if ((C & 0xF8) == 0xF0)
		{
			OutByteCount = 4;
			return 1.05f;
		}

		OutByteCount = 1;
		return 0.62f;
	}

	static FVector2 EstimateTextSize(const char* Text, float FontSize, float LetterSpacing)
	{
		if (!Text || Text[0] == '\0' || FontSize <= 0.0f)
		{
			return { 0.0f, 0.0f };
		}

		const float SafeFontSize = (std::max)(FontSize, 1.0f);
		const float SpacingScale = (std::max)(LetterSpacing, 0.0f);
		const float LineHeight = SafeFontSize * 1.2f;

		float CurrentLineWidth = 0.0f;
		float MaxLineWidth = 0.0f;
		int32 LineCount = 1;

		for (size_t Index = 0; Text[Index] != '\0';)
		{
			const unsigned char C = static_cast<unsigned char>(Text[Index]);
			if (C == static_cast<unsigned char>('\n'))
			{
				MaxLineWidth = (std::max)(MaxLineWidth, CurrentLineWidth);
				CurrentLineWidth = 0.0f;
				++LineCount;
				++Index;
				continue;
			}

			int32 ByteCount = 1;
			const float AdvanceUnit = EstimateGlyphAdvanceUnit(Text + Index, ByteCount);
			CurrentLineWidth += AdvanceUnit * SafeFontSize * SpacingScale;
			Index += static_cast<size_t>(ByteCount);
		}

		MaxLineWidth = (std::max)(MaxLineWidth, CurrentLineWidth);
		return { MaxLineWidth, LineHeight * static_cast<float>(LineCount) };
	}
}

void FSlatePaintContext::SetScreenSize(int32 Width, int32 Height)
{
	DrawList.ScreenWidth = (std::max)(0, Width);
	DrawList.ScreenHeight = (std::max)(0, Height);
}

bool FSlatePaintContext::HasActiveClip() const
{
	return !ClipStack.empty() && ClipStack.back().IsValid();
}

FRect FSlatePaintContext::ApplyCurrentClip(const FRect& InRect) const
{
	if (!HasActiveClip())
	{
		return InRect;
	}

	return IntersectRect(InRect, ClipStack.back());
}

void FSlatePaintContext::AppendElement(FUIDrawElement&& Element)
{
	Element.Layer = CurrentLayer;
	Element.Depth = CurrentDepth;
	Element.Order = NextOrder++;

	if (HasActiveClip())
	{
		Element.bHasClipRect = true;
		Element.ClipRect = ToUIRect(ClipStack.back());
	}

	DrawList.Elements.push_back(std::move(Element));
}

void FSlatePaintContext::DrawRectFilled(FRect Rect, uint32 Color)
{
	if (!Rect.IsValid())
	{
		return;
	}

	Rect = ApplyCurrentClip(Rect);
	if (!Rect.IsValid())
	{
		return;
	}

	FUIDrawElement Element;
	Element.Type = EUIDrawElementType::FilledRect;
	Element.Rect = ToUIRect(Rect);
	Element.Color = Color;
	AppendElement(std::move(Element));
}

void FSlatePaintContext::DrawRect(FRect Rect, uint32 Color)
{
	if (!Rect.IsValid())
	{
		return;
	}

	Rect = ApplyCurrentClip(Rect);
	if (!Rect.IsValid())
	{
		return;
	}

	FUIDrawElement Element;
	Element.Type = EUIDrawElementType::RectOutline;
	Element.Rect = ToUIRect(Rect);
	Element.Color = Color;
	AppendElement(std::move(Element));
}

void FSlatePaintContext::DrawText(FPoint Point, const char* Text, uint32 Color, float FontSize, float LetterSpacing)
{
	if (!Text || Text[0] == '\0' || FontSize <= 0.0f)
	{
		return;
	}

	const FVector2 EstimatedSize = EstimateTextSize(Text, FontSize, LetterSpacing);
	FRect TextRect(
		Point.X,
		Point.Y,
		static_cast<int32>(EstimatedSize.X + 0.5f),
		static_cast<int32>(EstimatedSize.Y + 0.5f));

	if (HasActiveClip() && !IntersectRect(TextRect, ClipStack.back()).IsValid())
	{
		return;
	}

	FUIDrawElement Element;
	Element.Type = EUIDrawElementType::Text;
	Element.Point = { static_cast<float>(Point.X), static_cast<float>(Point.Y) };
	Element.Rect = ToUIRect(TextRect);
	Element.Color = Color;
	Element.Text = Text;
	Element.FontSize = FontSize;
	Element.LetterSpacing = LetterSpacing;
	AppendElement(std::move(Element));
}

FVector2 FSlatePaintContext::MeasureText(const char* Text, float FontSize, float LetterSpacing)
{
	return EstimateTextSize(Text, FontSize, LetterSpacing);
}

void FSlatePaintContext::PushClipRect(const FRect& InRect)
{
	if (!InRect.IsValid())
	{
		ClipStack.push_back({ 0, 0, 0, 0 });
		return;
	}

	if (ClipStack.empty())
	{
		ClipStack.push_back(InRect);
		return;
	}

	ClipStack.push_back(IntersectRect(ClipStack.back(), InRect));
}

void FSlatePaintContext::PopClipRect()
{
	if (!ClipStack.empty())
	{
		ClipStack.pop_back();
	}
}

void FSlatePaintContext::PushDepth(float InDepth)
{
	DepthStack.push_back(CurrentDepth);
	CurrentDepth += InDepth;
}

void FSlatePaintContext::PopDepth()
{
	if (!DepthStack.empty())
	{
		CurrentDepth = DepthStack.back();
		DepthStack.pop_back();
	}
	else
	{
		CurrentDepth = 0.0f;
	}
}

void FSlatePaintContext::PushLayer(int32 InLayer)
{
	LayerStack.push_back(CurrentLayer);
	CurrentLayer += InLayer;
}

void FSlatePaintContext::PopLayer()
{
	if (!LayerStack.empty())
	{
		CurrentLayer = LayerStack.back();
		LayerStack.pop_back();
	}
	else
	{
		CurrentLayer = 0;
	}
}

FUIDrawList FSlatePaintContext::ConsumeDrawList()
{
	FUIDrawList Out = std::move(DrawList);
	Reset();
	return Out;
}

void FSlatePaintContext::Reset()
{
	DrawList.Clear();
	ClipStack.clear();
	LayerStack.clear();
	CurrentLayer = 0;
	DepthStack.clear();
	CurrentDepth = 0.0f;
	NextOrder = 0;
}
