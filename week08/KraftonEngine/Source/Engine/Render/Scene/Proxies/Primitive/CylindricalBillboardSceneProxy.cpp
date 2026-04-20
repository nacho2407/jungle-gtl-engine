#include "Render/Resources/ConstantBufferLayouts.h"
#include "Render/Types/PipelineStateTypes.h"
#include "Render/Scene/Proxies/Primitive/CylindricalBillboardSceneProxy.h"
#include "Component/CylindricalBillboardComponent.h"
#include "Render/Resources/ShaderManager.h"
#include "Render/Resources/MeshBufferManager.h"
#include "Render/Pipelines/Context/View/SceneView.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"

// ============================================================
// FCylindricalBillboardSceneProxy
// ============================================================
FCylindricalBillboardSceneProxy::FCylindricalBillboardSceneProxy(UCylindricalBillboardComponent* InComponent)
    : FBillboardSceneProxy(InComponent)
{
    // 컬링 문제일 가능성을 배제하기 위해 NoCull 설정
    Rasterizer = ERasterizerState::SolidNoCull;
}

void FCylindricalBillboardSceneProxy::UpdateMesh()
{
    // 기본 BillboardSceneProxy의 UpdateMesh를 그대로 따르되,
    // UCylindricalBillboardComponent용으로 캐스팅하여 호출하거나 동일 로직 수행.
    // FBillboardSceneProxy::UpdateMesh()는 GetBillboardComponent()를 사용하므로
    // 여기서 직접 호출해도 UCylindricalBillboardComponent가 UBillboardComponent를 상속하므로 잘 동작함.
    FBillboardSceneProxy::UpdateMesh();
}

void FCylindricalBillboardSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
    UCylindricalBillboardComponent* Comp = static_cast<UCylindricalBillboardComponent*>(Owner);
    bVisible = Comp->IsVisible();
    if (!bVisible)
        return;

    // Update DiffuseSRV (material or texture may have changed)
    UMaterial* Mat = Comp->GetMaterial();
    if (Mat)
    {
        UTexture2D* DiffuseTex = nullptr;
        if (Mat->GetTextureParameter("DiffuseTexture", DiffuseTex))
        {
            DiffuseSRV = DiffuseTex->GetSRV();
        }
    }

    FVector BillboardPos = Comp->GetWorldLocation();
    FVector BillboardForward = Frame.CameraForward * 1.0f;

    // 로컬 축 구하기
    FVector LocalAxis = Comp->GetBillboardAxis();
    if (LocalAxis.Dot(LocalAxis) < 0.0001f)
    {
        LocalAxis = FVector(0, 0, 1);
    }
    else
    {
        LocalAxis.Normalize();
    }

    // 빌보드 이전의 순수 월드 회전축 계산
    FMatrix NonBillboardWorldMatrix;
    if (Comp->GetParent())
    {
        NonBillboardWorldMatrix = Comp->GetRelativeMatrix() * Comp->GetParent()->GetWorldMatrix();
    }
    else
    {
        NonBillboardWorldMatrix = Comp->GetRelativeMatrix();
    }

    FVector WorldAxis = NonBillboardWorldMatrix.TransformVector(LocalAxis).Normalized();

    // 카메라 방향을 축에 수직인 평면에 투영
    FVector Forward = BillboardForward - (WorldAxis * BillboardForward.Dot(WorldAxis));
    if (Forward.Dot(Forward) < 0.0001f)
    {
        FVector TempUp = (std::abs(WorldAxis.Dot(FVector(0, 0, 1))) < 0.99f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
        Forward = TempUp.Cross(WorldAxis).Normalized();
    }
    else
    {
        Forward.Normalize();
    }

    FVector Right = WorldAxis.Cross(Forward).Normalized();
    FVector Up = WorldAxis;

    FMatrix RotMatrix;
    RotMatrix.SetAxes(Forward, Right, Up);

    FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(Comp->GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(BillboardPos);

    PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
    MarkPerObjectCBDirty();
}
