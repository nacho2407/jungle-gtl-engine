#include "Widget.h"

bool SWidget::IsHover(FPoint Point) const
{
	if (!Rect.IsValid()) return false;

	return (Rect.X < Point.X && Point.X < Rect.X + Rect.Width &&
		Rect.Y < Point.Y && Point.Y < Rect.Y + Rect.Height);
}

FVector2 SWidget::MeasureText(const char* Text, float FontSize, float LetterSpacing, FDynamicMesh*& InOutMesh)
{
	(void)Text;
	(void)FontSize;
	(void)LetterSpacing;
	(void)InOutMesh;
	return { 0.0f, 0.0f };
}
