#include "FSceneViewport.h"
#include "Render/Renderer/RenderTarget/RenderTargetFactory.h"
#include "Render/Renderer/RenderTarget/DepthStencilFactory.h"
#include <algorithm>


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

AActor* FSceneViewport::GetEditorIdPickActor(uint32 PickId) const
{
    if (PickId == 0)
    {
        return nullptr;
    }

    const uint32 ActorIndex = PickId - 1u;
    if (ActorIndex >= EditorIdPickActors.size())
    {
        return nullptr;
    }
    return EditorIdPickActors[ActorIndex];
}

bool FSceneViewport::ReadEditorIdPickAt(uint32 X, uint32 Y, ID3D11DeviceContext* Context, uint32& OutId) const
{
    OutId = 0;
    if (!RenderTargetSet || !Context || !RenderTargetSet->EditorIdPickTexture || !RenderTargetSet->EditorIdPickReadbackTexture)
    {
        return false;
    }

    const uint32 Width = static_cast<uint32>(std::max(0.0f, RenderTargetSet->Width));
    const uint32 Height = static_cast<uint32>(std::max(0.0f, RenderTargetSet->Height));
    if (Width == 0 || Height == 0)
    {
        return false;
    }

    const uint32 ClampedX = std::min(X, Width - 1u);
    const uint32 ClampedY = std::min(Y, Height - 1u);

    D3D11_BOX SourceBox = {};
    SourceBox.left = ClampedX;
    SourceBox.right = ClampedX + 1u;
    SourceBox.top = ClampedY;
    SourceBox.bottom = ClampedY + 1u;
    SourceBox.front = 0;
    SourceBox.back = 1;

    Context->CopySubresourceRegion(
        RenderTargetSet->EditorIdPickReadbackTexture,
        0,
        0,
        0,
        0,
        RenderTargetSet->EditorIdPickTexture,
        0,
        &SourceBox);

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (FAILED(Context->Map(RenderTargetSet->EditorIdPickReadbackTexture, 0, D3D11_MAP_READ, 0, &Mapped)))
    {
        return false;
    }

    OutId = *reinterpret_cast<const uint32*>(Mapped.pData);
    Context->Unmap(RenderTargetSet->EditorIdPickReadbackTexture, 0);
    return true;
}
