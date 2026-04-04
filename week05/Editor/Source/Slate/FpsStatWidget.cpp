#include "FpsStatWidget.h"

#include "EditorEngine.h"
#include <algorithm>
#include <cstdio>

namespace
{
	FString BuildFpsText(float InFPS, float InFrameTimeMs, uint32 InDrawCallCount, FEditorEngine*& Engine)
	{
		char Buffer[96];
		std::snprintf(Buffer, sizeof(Buffer),
			"FPS: %.1f (%.3f ms) | Draw: %u | Picking: %.3f ms | Count: %u | Total: %.3f ms",
			InFPS, InFrameTimeMs, InDrawCallCount, (float)Engine->LastPickTime, Engine->TotalPickCount, (float)Engine->TotalPickTime);
		Buffer[sizeof(Buffer) - 1] = '\0';
		return FString(Buffer);
	}
}

FpsStatWidget::FpsStatWidget(FEditorEngine* InEngine)
	: Engine(InEngine)
{
	FpsTextBlock.FontSize = FontSize;
	FpsTextBlock.SetText(BuildFpsText(0.0f, 0.0f, 0, Engine));
	FpsTextBlock.LetterSpacing = 0.5f;
}

void FpsStatWidget::OnPaint(SWidget& Painter)
{
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
	if (FRenderer* Renderer = Engine->GetRenderer())
	{
		DrawCallCount = Renderer->GetFrameDrawCallCount();
	}
	else
	{
		DrawCallCount = 0;
	}

	FpsTextBlock.SetText(BuildFpsText(FPS, FrameTimeMs, DrawCallCount, Engine));
}
