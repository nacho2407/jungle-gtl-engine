#include "Renderer/D3D11/D3D11OverlayMeshRenderer.h"

bool FD3D11OverlayMeshRenderer::Initialize(FD3D11RHI* InRHI)
{
    return MeshBatchRenderer.Initialize(InRHI);
}

void FD3D11OverlayMeshRenderer::Shutdown()
{
    MeshBatchRenderer.Shutdown();
}

void FD3D11OverlayMeshRenderer::BeginFrame(const FSceneView* InSceneView,
                                           EViewModeIndex    InViewMode,
                                           bool              bInUseInstancing)
{
    MeshBatchRenderer.BeginFrame(InSceneView, InViewMode, bInUseInstancing);
}

void FD3D11OverlayMeshRenderer::AddPrimitive(const FPrimitiveRenderItem& InItem)
{
    MeshBatchRenderer.AddPrimitive(InItem, EMeshDrawClass::Editor);
}

void FD3D11OverlayMeshRenderer::AddPrimitives(const TArray<FPrimitiveRenderItem>& InItems)
{
    MeshBatchRenderer.AddPrimitives(InItems, EMeshDrawClass::Editor);
}

void FD3D11OverlayMeshRenderer::EndFrame()
{
    MeshBatchRenderer.EndFrame();
}

void FD3D11OverlayMeshRenderer::Flush()
{
    MeshBatchRenderer.Flush();
}
