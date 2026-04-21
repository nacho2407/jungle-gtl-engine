#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"

#include "Component/TextRenderComponent.h"
#include "Render/Pipelines/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/ConstantBufferLayouts.h"
#include "Render/Resources/Shaders/ShaderManager.h"

FTextRenderSceneProxy::FTextRenderSceneProxy(UTextRenderComponent* InComponent)
    : FPrimitiveSceneProxy(InComponent)
{
    bPerViewportUpdate = true;
    bShowAABB = false;
    bAllowViewModeShaderOverride = false;
}

void FTextRenderSceneProxy::UpdateMesh()
{
    MeshBuffer = Owner ? Owner->GetMeshBuffer() : nullptr;
    Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
    Pass = ERenderPass::AlphaBlend;
    Blend = EBlendState::AlphaBlend;
    DepthStencil = EDepthStencilState::DepthReadOnly;
    Rasterizer = ERasterizerState::SolidNoCull;
    bFontBatched = true;
}

UTextRenderComponent* FTextRenderSceneProxy::GetTextRenderComponent() const
{
    return static_cast<UTextRenderComponent*>(Owner);
}

void FTextRenderSceneProxy::UpdatePerViewport(const FSceneView& SceneView)
{
    UTextRenderComponent* TextComp = GetTextRenderComponent();
    if (!TextComp)
    {
        bVisible = false;
        return;
    }

    if (TextComp->GetText().empty() || !TextComp->GetFont() || !TextComp->GetFont()->IsLoaded())
    {
        bVisible = false;
        return;
    }

    if (!SceneView.ShowFlags.bBillboardText)
    {
        bVisible = false;
        return;
    }

    bVisible = TextComp->IsVisible();
    if (!bVisible)
    {
        return;
    }

    if (TextComp->IsBillboard())
    {
        FVector BillboardForward = SceneView.CameraForward * -1.0f;
        FMatrix RotMatrix;
        RotMatrix.SetAxes(BillboardForward, SceneView.CameraRight * -1.0f, SceneView.CameraUp);
        CachedTextWorldMatrix = FMatrix::MakeScaleMatrix(TextComp->GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(TextComp->GetWorldLocation());
        CachedTextRight = SceneView.CameraRight * -1.0f;
        CachedTextUp = SceneView.CameraUp;
    }
    else
    {
        CachedTextWorldMatrix = TextComp->GetWorldMatrix();
        CachedTextRight = TextComp->GetRightVector();
        CachedTextUp = TextComp->GetUpVector();
    }

    CachedText = TextComp->GetText();
    CachedFontScale = TextComp->GetFontSize();

    const FMatrix OutlineMatrix = TextComp->CalculateOutlineMatrix(CachedTextWorldMatrix);
    PerObjectConstants = FPerObjectConstants::FromWorldMatrix(OutlineMatrix);
    MarkPerObjectCBDirty();
}
