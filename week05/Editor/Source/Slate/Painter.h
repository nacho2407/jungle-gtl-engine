#pragma once

#include "Widget.h"
#include "Renderer/RenderCommand.h"

class FRenderer;
struct FRenderCommandQueue;

class FPainter : public SWidget
{
public:
	explicit FPainter(FRenderer* InRenderer);

	void SetScreenSize(int32 Width, int32 Height);

	void DrawRectFilled(FRect Rect, uint32 Color) override;
	void DrawRect(FRect Rect, uint32 Color) override;
	void DrawText(FPoint Point, const char* Text, uint32 Color) override;

	void Flush();

private:
	FRenderer* Renderer;
	FMatrix OrthoProj;
	FRenderCommandQueue UIQueue;
};