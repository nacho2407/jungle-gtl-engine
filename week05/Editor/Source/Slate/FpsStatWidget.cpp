#include "FpsStatWidget.h"

#include "EditorEngine.h"
#include <algorithm>
#include <cstdio>

namespace
{
	FString BuildFpsText(float InFPS, float InFrameTimeMs)
	{
		char Buffer[64];
		std::snprintf(Buffer, sizeof(Buffer), "FPS: %.1f (%.3f ms)", InFPS, InFrameTimeMs);
		Buffer[sizeof(Buffer) - 1] = '\0';
		return FString(Buffer);
	}
}

FpsStatWidget::FpsStatWidget(FEditorEngine* InEngine)
	: Engine(InEngine)
{
	FpsTextBlock.FontSize = FontSize;
	FpsTextBlock.SetText(BuildFpsText(0.0f, 0.0f));
	FpsTextBlock.LetterSpacing = 0.5f;
}

void FpsStatWidget::OnPaint(FSlatePaintContext& Painter)
{
	if (!bVisible)
	{
		return;
	}

	UpdateGeometry();

	if (!Rect.IsValid())
	{
		return;
	}

	FpsTextBlock.Paint(Painter);
}

void FpsStatWidget::SetWidgetRect(const FRect& InRect)
{
	Rect = InRect;
	UpdateGeometry();
}

void FpsStatWidget::Refresh()
{
	SyncValue();
}

int32 FpsStatWidget::GetDesiredWidth() const
{
	if (!bVisible)
	{
		return 0;
	}

	const FVector2 DesiredSize = FpsTextBlock.ComputeDesiredSize();
	return static_cast<int32>(DesiredSize.X + 0.5f) + Gap;
}

void FpsStatWidget::UpdateGeometry()
{
	if (!Rect.IsValid())
	{
		Rect = { 0, 0, 0, 0 };
		FpsTextBlock.Rect = { 0, 0, 0, 0 };
		return;
	}

	const FVector2 DesiredSize = FpsTextBlock.ComputeDesiredSize();
	const int32 TextWidth = static_cast<int32>(DesiredSize.X + 0.5f);
	const int32 TextHeight = static_cast<int32>(DesiredSize.Y + 0.5f);
	const int32 FontPixelHeight = static_cast<int32>(FontSize + 0.5f);
	const int32 RowY = Rect.Y + (Rect.Height - FontPixelHeight) / 2;
	FpsTextBlock.Rect = { Rect.X, RowY, TextWidth, TextHeight };
}

void FpsStatWidget::SyncValue()
{
	if (!Engine)
	{
		return;
	}

	const FTimer& Timer = Engine->GetTimer();
	FPS = Timer.GetDisplayFPS();
	FrameTimeMs = Timer.GetFrameTimeMs();
	FpsTextBlock.SetText(BuildFpsText(FPS, FrameTimeMs));
}

