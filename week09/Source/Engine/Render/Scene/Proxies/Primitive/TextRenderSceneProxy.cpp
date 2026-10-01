// 렌더 영역의 세부 동작을 구현합니다.
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"

#include "Component/TextRenderComponent.h"
#include "Render/Execute/Context/Scene/SceneView.h"
#include "Render/Resources/Buffers/ConstantBufferData.h"
#include "Render/Resources/Shaders/ShaderManager.h"

FTextRenderSceneProxy::FTextRenderSceneProxy(UTextRenderComponent* InComponent)
    : FPrimitiveProxy(InComponent)
{
    bPerViewportUpdate           = true;
    bShowAABB                    = false;
    bAllowViewModeShaderOverride = false;
}

void FTextRenderSceneProxy::UpdateMesh()
{
    MeshBuffer   = Owner ? Owner->GetMeshBuffer() : nullptr;
    Shader       = FShaderManager::Get().GetShader(EShaderType::Primitive);
    Pass         = ERenderPass::AlphaBlend;
    Blend        = EBlendState::AlphaBlend;
    DepthStencil = EDepthStencilState::DepthReadOnly;
    Rasterizer   = ERasterizerState::SolidNoCull;
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

    if (TextComp->GetText().empty() || !TextComp->ReacquireDefaultFont())
    {
        bVisible = false;
        return;
    }

    const bool bShowText = TextComp->UsesUUIDTextShowFlag()
                               ? SceneView.ShowFlags.bUUIDText
                               : SceneView.ShowFlags.bText;
    if (!bShowText)
    {
        bVisible = false;
        return;
    }

    bVisible = TextComp->ShouldRenderInCurrentWorld();
    if (!bVisible)
    {
        return;
    }

    if (TextComp->IsBillboard())
    {
        CachedTextWorldMatrix = TextComp->CalculateBillboardWorldMatrix(SceneView.CameraForward);
        CachedTextRight       = FVector(CachedTextWorldMatrix.M[1][0], CachedTextWorldMatrix.M[1][1], CachedTextWorldMatrix.M[1][2]).Normalized();
        CachedTextUp          = FVector(CachedTextWorldMatrix.M[2][0], CachedTextWorldMatrix.M[2][1], CachedTextWorldMatrix.M[2][2]).Normalized();
    }
    else
    {
        CachedTextWorldMatrix = TextComp->GetWorldMatrix();
        CachedTextRight       = TextComp->GetRightVector();
        CachedTextUp          = TextComp->GetUpVector();
    }

    CachedText      = TextComp->GetText();
    CachedFontScale = TextComp->GetFontSize();

    const FMatrix OutlineMatrix = TextComp->CalculateOutlineMatrix(CachedTextWorldMatrix);
    PerObjectConstants          = FPerObjectCBData::FromWorldMatrix(OutlineMatrix);
    MarkPerObjectCBDirty();
}
