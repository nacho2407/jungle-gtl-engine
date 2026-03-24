#include "Renderer/D3D11/D3D11SceneMeshRenderer.h"

bool FD3D11SceneMeshRenderer::Initialize(FD3D11RHI* InRHI)
{
    return MeshBatchRenderer.Initialize(InRHI);
}

void FD3D11SceneMeshRenderer::Shutdown()
{
    MeshBatchRenderer.Shutdown();
}

void FD3D11SceneMeshRenderer::BeginFrame(const FSceneView* InSceneView,
                                         EViewModeIndex    InViewMode,
                                         bool              bInUseInstancing)
{
    MeshBatchRenderer.BeginFrame(InSceneView, InViewMode, bInUseInstancing);
}

void FD3D11SceneMeshRenderer::AddPrimitive(const FPrimitiveRenderItem& InItem)
{
    MeshBatchRenderer.AddPrimitive(InItem, EMeshDrawClass::Scene);
}

void FD3D11SceneMeshRenderer::AddPrimitives(const TArray<FPrimitiveRenderItem>& InItems)
{
    MeshBatchRenderer.AddPrimitives(InItems, EMeshDrawClass::Scene);
}

void FD3D11SceneMeshRenderer::EndFrame()
{
    MeshBatchRenderer.EndFrame();
}

void FD3D11SceneMeshRenderer::Flush()
{
    MeshBatchRenderer.Flush();
}
