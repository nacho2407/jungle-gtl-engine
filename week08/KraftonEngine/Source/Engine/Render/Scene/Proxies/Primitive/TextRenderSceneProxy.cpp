#include "Render/Resources/ConstantBufferLayouts.h"
#include "Render/Pipelines/RenderPassTypes.h"
#include "Render/Scene/Proxies/Primitive/TextRenderSceneProxy.h"
#include "Component/TextRenderComponent.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include "Render/Resources/ShaderManager.h"

// ============================================================
// FTextRenderSceneProxy
// ============================================================
FTextRenderSceneProxy::FTextRenderSceneProxy(UTextRenderComponent* InComponent)
    : FBillboardSceneProxy(static_cast<UBillboardComponent*>(InComponent))
{
}

void FTextRenderSceneProxy::UpdateMesh()
{
    // SelectionMask 아웃라인 패스에서 사용할 mesh/shader
    MeshBuffer = Owner->GetMeshBuffer();
    Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
    Pass = ERenderPass::AlphaBlend;
    bFontBatched = true;
}

UTextRenderComponent* FTextRenderSceneProxy::GetTextRenderComponent() const
{
    return static_cast<UTextRenderComponent*>(Owner);
}

// ============================================================
// UpdatePerViewport — 빌보드 행렬 계산 + 텍스트 데이터 캐싱
// ============================================================
void FTextRenderSceneProxy::UpdatePerViewport(const FSceneView& SceneView)
{
    UTextRenderComponent* TextComp = GetTextRenderComponent();

    // 텍스트/폰트 미설정 시 비가시
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
        return;

    // 빌보드 행렬
    FVector BillboardForward = SceneView.CameraForward * -1.0f;
    FMatrix RotMatrix;
    RotMatrix.SetAxes(BillboardForward, SceneView.CameraRight * -1.0f, SceneView.CameraUp);
    CachedBillboardMatrix = FMatrix::MakeScaleMatrix(TextComp->GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(TextComp->GetWorldLocation());

    // 텍스트 데이터 캐싱 (Collector가 FFontGeometry 배칭에 사용)
    CachedText = TextComp->GetText();
    CachedFontScale = TextComp->GetFontSize();

    // SelectionMask용 아웃라인 행렬 (텍스트 너비·높이 반영)
    FMatrix OutlineMatrix = TextComp->CalculateOutlineMatrix(CachedBillboardMatrix);
    PerObjectConstants = FPerObjectConstants::FromWorldMatrix(OutlineMatrix);
    MarkPerObjectCBDirty();
}
