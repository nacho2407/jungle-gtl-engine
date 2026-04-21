#include "Render/Resources/Buffers/ConstantBufferLayouts.h"
#include "Render/Scene/Proxies/Primitive/PrimitiveShapeTypes.h"
#include "Render/Passes/Base/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/SubUVSceneProxy.h"
#include "Component/SubUVComponent.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Resources/Shaders/ShaderManager.h"
#include "Render/Resources/Buffers/MeshBufferManager.h"

// ============================================================
// FSubUVSceneProxy
// ============================================================
FSubUVSceneProxy::FSubUVSceneProxy(USubUVComponent* InComponent)
    : FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
    bShowAABB = false;
}

FSubUVSceneProxy::~FSubUVSceneProxy()
{
    UVRegionCB.Release();
}

void FSubUVSceneProxy::UpdateMesh()
{
    USubUVComponent* Comp = GetSubUVComponent();

    // TexturedQuad (FVertexPNCT with UVs) for rendering
    MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::TexturedQuad);
    Shader = FShaderManager::Get().GetShader(EShaderType::SubUV);
    Pass = ERenderPass::AlphaBlend;

    // ExtraCB bind (UV region, b2 slot) — 실제 GPU 버퍼는 Renderer에서 lazy 생성
    ExtraCB.Bind<FSubUVRegionConstants>(&UVRegionCB, ECBSlot::PerShader0);

    // Set DiffuseSRV from particle resource
    const FParticleResource* Particle = Comp->GetParticle();
    if (Particle && Particle->IsLoaded())
    {
        DiffuseSRV = Particle->SRV;
    }
}

void FSubUVSceneProxy::UpdatePerViewport(const FSceneView& SceneView)
{
    USubUVComponent* Comp = GetSubUVComponent();
    bVisible = Comp->IsVisible();
    if (!bVisible)
        return;

    const FParticleResource* Particle = Comp->GetParticle();
    if (!Particle || !Particle->IsLoaded())
    {
        bVisible = false;
        return;
    }

    // Update DiffuseSRV (may change during play)
    DiffuseSRV = Particle->SRV;

    // Billboard matrix
    FVector BillboardForward = SceneView.CameraForward * -1.0f;
    FMatrix RotMatrix;
    RotMatrix.SetAxes(BillboardForward, SceneView.CameraRight, SceneView.CameraUp);
    FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(Comp->GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(Comp->GetWorldLocation());

    PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
    MarkPerObjectCBDirty();

    // Update UV region from frame index
    const uint32 Cols = Particle->Columns;
    const uint32 Rows = Particle->Rows;
    if (Cols > 0 && Rows > 0)
    {
        const float FrameW = 1.0f / static_cast<float>(Cols);
        const float FrameH = 1.0f / static_cast<float>(Rows);
        const uint32 FrameIdx = Comp->GetFrameIndex();
        const uint32 Col = FrameIdx % Cols;
        const uint32 Row = FrameIdx / Cols;

        FSubUVRegionConstants& Region = ExtraCB.As<FSubUVRegionConstants>();
        Region.U = Col * FrameW;
        Region.V = Row * FrameH;
        Region.Width = FrameW;
        Region.Height = FrameH;
    }
}

USubUVComponent* FSubUVSceneProxy::GetSubUVComponent() const
{
    return static_cast<USubUVComponent*>(Owner);
}
