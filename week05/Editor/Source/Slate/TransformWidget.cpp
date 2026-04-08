#include "TransformWidget.h"

#include "EditorEngine.h"
#include "Gizmo/Gizmo.h"
#include "Viewport/EditorViewportClient.h"
#include <algorithm>

FTransformWidget::FTransformWidget(FEditorEngine* InEngine, FEditorViewportClient* InViewportClient) :
	Engine(InEngine), ViewportClient(InViewportClient)
{
	Rect = { 0, 0, 0, 0 };

	TranslateModeButton.Text = "T";
	RotationModeButton.Text = "R";
	ScaleModeButton.Text = "S";
	ToggleCoordModeButton.Text = "W";

	TranslateModeButton.FontSize = 18.0f;
	RotationModeButton.FontSize = 18.0f;
	ScaleModeButton.FontSize = 18.0f;
	ToggleCoordModeButton.FontSize = 18.0f;

	TranslateModeButton.TextHAlign = ETextHAlign::Center;
	TranslateModeButton.TextVAlign = ETextVAlign::Center;

	TranslateModeButton.OnClicked = [this]() { SetTranslateMode(); };
	RotationModeButton.OnClicked = [this]() { SetRotationMode(); };
	ScaleModeButton.OnClicked = [this]() { SetScaleMode(); };
	ToggleCoordModeButton.OnClicked = [this]() { ToggleCoordMode(); };
}

FEditorViewportClient* FTransformWidget::GetActiveViewportClient() const
{
	return ViewportClient;
}

void FTransformWidget::OnPaint(FSlatePaintContext& Painter)
{
	UpdateGeometry();
	SyncSelectionState();

	if (!Rect.IsValid()) return;

	TranslateModeButton.Paint(Painter);
	RotationModeButton.Paint(Painter);
	ScaleModeButton.Paint(Painter);
	ToggleCoordModeButton.Paint(Painter);
}

bool FTransformWidget::OnMouseDown(int32 X, int32 Y)
{
	UpdateGeometry();
	SyncSelectionState();

	if (!HitTest({ X, Y })) return false;

	if (HandleButtonMouse(TranslateModeButton, X, Y)) return true;
	if (HandleButtonMouse(RotationModeButton, X, Y)) return true;
	if (HandleButtonMouse(ScaleModeButton, X, Y)) return true;
	if (HandleButtonMouse(ToggleCoordModeButton, X, Y)) return true;
	return false;
}

bool FTransformWidget::HitTest(FPoint Point) const
{
	return ContainsPoint(GetExpandedInteractiveRect(), Point);
}

void FTransformWidget::SetWidgetRect(const FRect& InRect)
{
	Rect = InRect;
	UpdateGeometry();
}

FRect FTransformWidget::GetInteractiveRect() const
{
	return GetExpandedInteractiveRect();
}

int32 FTransformWidget::GetDesiredWidth() const
{
	return Padding * 2 + ButtonSize * 4 + Gap * 3;
}

void FTransformWidget::SyncSelectionState()
{
	FEditorViewportClient* ActiveClient = GetActiveViewportClient();
	const bool bEnabled = (ActiveClient != nullptr);
	if (!bEnabled)
	{
		TranslateModeButton.bEnabled = false;
		RotationModeButton.bEnabled = false;
		ScaleModeButton.bEnabled = false;
		ToggleCoordModeButton.bEnabled = false;
		return;
	}

	const EGizmoMode Mode = ActiveClient->GetGizmoMode();
	auto Configure = [bEnabled](SButton& Button, bool bActive)
		{
			Button.bEnabled = bEnabled;
			Button.BackgroundColor = bActive ? 0xFF3B5E84 : 0xFF2C2F33;
			Button.BorderColor = bActive ? 0xFF86C8FF : 0xFF5A6068;
			Button.TextColor = 0xFFFFFFFF;
			Button.DisabledBackgroundColor = 0xFF1F2124;
			Button.DisabledTextColor = 0xFF757575;
		};

	Configure(TranslateModeButton, Mode == EGizmoMode::Location);
	Configure(RotationModeButton, Mode == EGizmoMode::Rotation);
	Configure(ScaleModeButton, Mode == EGizmoMode::Scale);
	Configure(ToggleCoordModeButton, true);

	switch (ActiveClient->GetSpaceMode())
	{
	case EGizmoCoordinateSpace::World:
		ToggleCoordModeButton.Text = "W";
		break;
	case EGizmoCoordinateSpace::Local:
		ToggleCoordModeButton.Text = "L";
		break;
	default:
		break;
	}
}

void FTransformWidget::UpdateGeometry()
{
	if (!Rect.IsValid())
	{
		TranslateModeButton.Rect = { 0, 0, 0, 0 };
		RotationModeButton.Rect = { 0, 0, 0, 0 };
		ScaleModeButton.Rect = { 0, 0, 0, 0 };
		ToggleCoordModeButton.Rect = { 0, 0, 0, 0 };
		return;
	}

	const int32 AvailableWidth = (std::max)(Rect.Width, ButtonSize * 4);
	const int32 LocalGap = (std::max)(2, (AvailableWidth - ButtonSize * 4 - Padding * 2) / 3);
	const int32 RowY = Rect.Y + (Rect.Height - ButtonSize) / 2;
	int32 CursorX = Rect.X + Padding;

	TranslateModeButton.Rect = { CursorX, RowY, ButtonSize, ButtonSize };
	CursorX += ButtonSize + LocalGap;
	RotationModeButton.Rect = { CursorX, RowY, ButtonSize, ButtonSize };
	CursorX += ButtonSize + LocalGap;
	ScaleModeButton.Rect = { CursorX, RowY, ButtonSize, ButtonSize };
	CursorX += ButtonSize + LocalGap;
	ToggleCoordModeButton.Rect = { CursorX, RowY, ButtonSize, ButtonSize };
}

void FTransformWidget::SetTranslateMode()
{
	if (FEditorViewportClient* ActiveClient = GetActiveViewportClient())
	{
		ActiveClient->SetGizmoMode(EGizmoMode::Location);
	}
}

void FTransformWidget::SetRotationMode()
{
	if (FEditorViewportClient* ActiveClient = GetActiveViewportClient())
	{
		ActiveClient->SetGizmoMode(EGizmoMode::Rotation);
	}
}

void FTransformWidget::SetScaleMode()
{
	if (FEditorViewportClient* ActiveClient = GetActiveViewportClient())
	{
		ActiveClient->SetGizmoMode(EGizmoMode::Scale);
	}
}

void FTransformWidget::ToggleCoordMode()
{
	if (FEditorViewportClient* ActiveClient = GetActiveViewportClient())
	{
		EGizmoCoordinateSpace Space = static_cast<EGizmoCoordinateSpace>(((int8)ActiveClient->GetSpaceMode() + 1) % 2);
		ActiveClient->SetSpaceMode(Space);
	}
}

bool FTransformWidget::HandleButtonMouse(SButton& Button, int32 X, int32 Y)
{
	return Button.OnMouseDown(X, Y);
}

FRect FTransformWidget::GetExpandedInteractiveRect() const
{
	FRect Expanded = Rect;
	Expanded = UnionRect(Expanded, TranslateModeButton.Rect);
	Expanded = UnionRect(Expanded, RotationModeButton.Rect);
	Expanded = UnionRect(Expanded, ScaleModeButton.Rect);
	Expanded = UnionRect(Expanded, ToggleCoordModeButton.Rect);
	return Expanded;
}

