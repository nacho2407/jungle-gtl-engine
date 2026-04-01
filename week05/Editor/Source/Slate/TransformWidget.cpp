#include "TransformWidget.h"

#include "EditorEngine.h"
#include "Gizmo/Gizmo.h"
#include "Viewport/EditorViewportClient.h"

FTransformWidget::FTransformWidget(FEditorEngine* InEngine, FEditorViewportClient* InViewportClient) :
	Engine(InEngine), ViewportClient(InViewportClient)
{
	Rect = { 0, 0, 0, 0 };

	TranslateModeButton.Text = "T";
	RotationModeButton.Text = "R";
	ScaleModeButton.Text = "S";
	ToggleCoordModeButton.Text = "L";

	TranslateModeButton.FontSize = 24.0f;
	RotationModeButton.FontSize = 24.0f;
	ScaleModeButton.FontSize = 24.0f;
	ToggleCoordModeButton.FontSize = 24.0f;

	TranslateModeButton.OnClicked = [this]() { SetTranslateMode(); };
	RotationModeButton.OnClicked = [this]() { SetRotationMode(); };
	ScaleModeButton.OnClicked = [this]() { SetScaleMode(); };
	ToggleCoordModeButton.OnClicked = [this]() { ToggleCoordMode(); };
}

void FTransformWidget::OnPaint(SWidget& Painter)
{
	UpdateGeometry();
	SyncSelectionState();

	if (!Rect.IsValid()) return;

	Painter.DrawRectFilled(Rect, 0xD01C1E21);
	Painter.DrawRect(Rect, 0xFF555B63);

	TranslateModeButton.Paint(Painter);
	RotationModeButton.Paint(Painter);
	ScaleModeButton.Paint(Painter);
	ToggleCoordModeButton.Paint(Painter);
}

bool FTransformWidget::OnMouseDown(int32 X, int32 Y)
{
	UpdateGeometry();
	SyncSelectionState();

	const FPoint Point({ X, Y });
	if (!HitTest(Point)) return false;

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
	return Padding * 3 + ButtonSize * 4 + Gap * 2;
}

void FTransformWidget::SyncSelectionState()
{
	const bool bEnabled = (ViewportClient != nullptr);

	EGizmoMode Mode = EGizmoMode::Location;
	if (ViewportClient) Mode = ViewportClient->GetGizmoMode();
	else return;

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

	ToggleCoordModeButton.bEnabled = true;
	ToggleCoordModeButton.BackgroundColor = 0xFF3B5E84;
	ToggleCoordModeButton.BorderColor = 0xFF86C8FF;
	ToggleCoordModeButton.TextColor = 0xFFFFFFFF;
	EGizmoCoordinateSpace Space = ViewportClient->GetSpaceMode();
	ViewportClient->SetSpaceMode(Space);
	switch (Space)
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
		Rect = { 0, 0, 0, 0 };
		TranslateModeButton.Rect = { 0, 0, 0, 0 };
		RotationModeButton.Rect = { 0, 0, 0, 0 };
		ScaleModeButton.Rect = { 0, 0, 0, 0 };
		ToggleCoordModeButton.Rect = { 0, 0, 0, 0 };
		return;
	}

	const int32 RowY = Rect.Y + (Rect.Height - ButtonSize) / 2;
	int32 CursorX = Rect.X + Padding;

	TranslateModeButton.Rect = { CursorX, RowY, ButtonSize, ButtonSize };
	CursorX += ButtonSize + Gap;

	RotationModeButton.Rect = { CursorX, RowY, ButtonSize, ButtonSize };
	CursorX += ButtonSize + Gap;

	ScaleModeButton.Rect = { CursorX, RowY, ButtonSize, ButtonSize };
	CursorX += ButtonSize + Gap;

	ToggleCoordModeButton.Rect = { CursorX, RowY, ButtonSize, ButtonSize };
}

void FTransformWidget::SetTranslateMode()
{
	ViewportClient->SetGizmoMode(EGizmoMode::Location);
}

void FTransformWidget::SetRotationMode()
{
	ViewportClient->SetGizmoMode(EGizmoMode::Rotation);
}

void FTransformWidget::SetScaleMode()
{
	ViewportClient->SetGizmoMode(EGizmoMode::Scale);
}

void FTransformWidget::ToggleCoordMode()
{
	EGizmoCoordinateSpace Space = static_cast<EGizmoCoordinateSpace>(((int8)ViewportClient->GetSpaceMode() + 1) % 2);
	ViewportClient->SetSpaceMode(Space);
	switch (Space)
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

bool FTransformWidget::HandleButtonMouse(SButton& Button, int32 X, int32 Y)
{
	return Button.OnMouseDown(X, Y);
}

FRect FTransformWidget::GetExpandedInteractiveRect() const
{
	FRect Expanded = Rect;

	Expanded = UnionRects(Expanded, TranslateModeButton.Rect);
	Expanded = UnionRects(Expanded, RotationModeButton.Rect);
	Expanded = UnionRects(Expanded, ScaleModeButton.Rect);
	Expanded = UnionRects(Expanded, ToggleCoordModeButton.Rect);

	return Expanded;
}

bool FTransformWidget::ContainsPoint(const FRect& InRect, FPoint Point)
{
	return InRect.IsValid() && InRect.X < Point.X && Point.X < InRect.X + InRect.Width &&
		InRect.Y < Point.Y && Point.Y < InRect.Y + InRect.Height;
}

FRect FTransformWidget::UnionRects(const FRect& A, const FRect& B)
{
	if (!A.IsValid()) return B;
	if (!B.IsValid()) return A;

	const int32 Left = (std::min)(A.X, B.X);
	const int32 Top = (std::min)(A.Y, B.Y);
	const int32 Right = (std::max)(A.X + A.Width, B.X + B.Width);
	const int32 Bottom = (std::max)(A.Y + A.Height, B.Y + B.Height);

	return { Left, Top, Right - Left, Bottom - Top };
}
