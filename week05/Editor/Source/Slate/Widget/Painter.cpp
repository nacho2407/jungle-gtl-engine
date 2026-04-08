#include "Painter.h"
#include "TextMetrics.h"

#include <algorithm>
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

	const FVector2 EstimatedSize = SWidgetTextMetrics::MeasureText(Text, FontSize, LetterSpacing);
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
	return SWidgetTextMetrics::MeasureText(Text, FontSize, LetterSpacing);
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
