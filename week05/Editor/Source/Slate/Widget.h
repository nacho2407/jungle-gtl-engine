#pragma once

#include "Viewport/ViewportTypes.h"
#include "Renderer/MeshData.h"

#ifdef DrawText
#undef DrawText
#endif

enum class EMouseCursor
{
	Default,
	None,
	TextEdit,
	ResizeLeftRight,
	ResizeUpDown,
	Hand,
};

class SWidget
{
public:
	virtual ~SWidget() {}

	FRect Rect;

	void Paint(SWidget& Painter) { OnPaint(Painter); }
	virtual void OnPaint(SWidget& Painter) {};
	virtual FVector2 ComputeDesiredSize() const { return { 0, 0 }; }
	virtual void ArrangeChildren() {}
	virtual bool IsHover(FPoint Point) const;

	virtual void DrawRectFilled(FRect Rect, uint32 Color) {}
	virtual void DrawRect(FRect Rect, uint32 Color) {}
	virtual void DrawText(FPoint Point, const char* Text, uint32 Color, float FontSize, FDynamicMesh*& InOutMesh) {}

	virtual EMouseCursor GetCursor() const { return EMouseCursor::Default; }
};