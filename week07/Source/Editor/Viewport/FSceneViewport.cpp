#include "FSceneViewport.h"
#include "Render/Renderer/RenderTarget/RenderTargetFactory.h"
#include "Render/Renderer/RenderTarget/DepthStencilFactory.h"


void FSceneViewport::Draw()
{
}

bool FSceneViewport::ContainsPoint(int32 X, int32 Y) const
{
    return FViewport::ContainsPoint(X, Y);
}

void FSceneViewport::WindowToLocal(int32 X, int32 Y, int32& OutX, int32& OutY) const
{
    FViewport::WindowToLocal(X, Y, OutX, OutY);
}

bool FSceneViewport::OnMouseMove(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseButtonDown(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseButtonUp(const FViewportMouseEvent& Ev)
{
	return false;
}

bool FSceneViewport::OnMouseWheel(float Delta)
{
	return false;
}

bool FSceneViewport::OnKeyDown(uint32 Key)
{
	return false;
}

bool FSceneViewport::OnKeyUp(uint32 Key)
{
	return false;
}

bool FSceneViewport::OnChar(uint32 Codepoint)
{
	return false;
}


FRenderTargetSet FSceneViewport::GetViewportRenderTargets() const
{
    if (RenderTargetSet)
        return *RenderTargetSet;

	// 자원 참조 없을 시 Default 반환
    return FRenderTargetSet();
}
