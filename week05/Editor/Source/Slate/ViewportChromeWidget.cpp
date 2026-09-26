#include "ViewportChromeWidget.h"
#include "UI/EditorUI.h"

FVector2 SViewportChromeWidget::ComputeDesiredSize() const
{
	const FVector2 Desired = Toolbar.ComputeDesiredSize();
	return { Desired.X + 12.0f, (std::max)(34.0f, Desired.Y + 8.0f) };
}

FVector2 SViewportChromeWidget::ComputeMinSize() const
{
	const FVector2 MinSize = Toolbar.ComputeMinSize();
	return { MinSize.X + 12.0f, (std::max)(34.0f, MinSize.Y + 8.0f) };
}

SViewportChromeWidget::SViewportChromeWidget(FEditorEngine* InEngine, FEditorUI* InEditorUI, FEditorViewportClient* InViewportClient, FViewportId InViewportId)
	: EditorUI(InEditorUI)
	, ViewSettings(InEngine)
	, FpsWidget(InEngine)
{
	(void)InViewportClient;
	ViewSettings.ConfigureForViewport(InViewportId);
	Toolbar.AddWidget(&ViewSettings, 1.0f).SetMinWidth(96.0f);
	Toolbar.AddWidget(&FpsWidget, 0.0f, FMargin(8.0f, 0.0f));
}

void SViewportChromeWidget::ArrangeChildren()
{
	if (!Rect.IsValid())
	{
		Toolbar.Rect = { 0, 0, 0, 0 };
		return;
	}

	SyncState();
	Toolbar.Rect = { Rect.X + 6, Rect.Y + 4, Rect.Width - 12, Rect.Height - 8 };
	Toolbar.ArrangeChildren();
}

void SViewportChromeWidget::OnPaint(FSlatePaintContext& Painter)
{
	if (!Rect.IsValid())
	{
		return;
	}

	SyncState();
	Painter.DrawRectFilled(Rect, BackgroundFillColor);
	Painter.DrawRect(Rect, BorderColor);
	Toolbar.Paint(Painter);
}

bool SViewportChromeWidget::OnMouseDown(int32 X, int32 Y)
{
	return Rect.IsValid() ? Toolbar.OnMouseDown(X, Y) : false;
}

bool SViewportChromeWidget::HitTest(FPoint Point) const
{
	return Toolbar.HitTest(Point);
}

bool SViewportChromeWidget::ShouldShowFPS() const
{
	return EditorUI && EditorUI->GetDebugState().FPS;
}

void SViewportChromeWidget::SyncState()
{
	const bool bShowFPS = ShouldShowFPS();
	FpsWidget.SetVisible(bShowFPS);
	if (bShowFPS)
	{
		FpsWidget.Refresh();
	}
}

