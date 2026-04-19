#include "Render/Proxy/CylindricalBillboardSceneProxy.h"
#include "Component/CylindricalBillboardComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Pipeline/FrameContext.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"

// ============================================================
// FCylindricalBillboardSceneProxy
// ============================================================
FCylindricalBillboardSceneProxy::FCylindricalBillboardSceneProxy(UCylindricalBillboardComponent* InComponent)
	: FBillboardSceneProxy(InComponent)
{
}

void FCylindricalBillboardSceneProxy::UpdateTransform()
{
	FBillboardSceneProxy::UpdateTransform();
	UCylindricalBillboardComponent* Comp = static_cast<UCylindricalBillboardComponent*>(GetOwner());

	FVector LocalAxis = Comp->GetBillboardAxis();
	if (LocalAxis.Dot(LocalAxis) < 0.0001f)
	{
		LocalAxis = FVector(0, 0, 1);
	}
	else
	{
		LocalAxis.Normalize();
	}

	FMatrix NonBillboardWorldMatrix;
	if (Comp->GetParent())
	{
		NonBillboardWorldMatrix = Comp->GetRelativeMatrix() * Comp->GetParent()->GetWorldMatrix();
	}
	else
	{
		NonBillboardWorldMatrix = Comp->GetRelativeMatrix();
	}

	CachedWorldAxis = NonBillboardWorldMatrix.TransformVector(LocalAxis).Normalized();
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
	if (!bVisible) return;

	FVector BillboardForward = Frame.CameraForward * 1.0f;

	// 카메라 방향을 축에 수직인 평면에 투영
	FVector Forward = BillboardForward - (CachedWorldAxis * BillboardForward.Dot(CachedWorldAxis));
	if (Forward.Dot(Forward) < 0.0001f)
	{
		FVector TempUp = (std::abs(CachedWorldAxis.Dot(FVector(0, 0, 1))) < 0.99f) ? FVector(0, 0, 1) : FVector(0, 1, 0);
		Forward = TempUp.Cross(CachedWorldAxis).Normalized();
	}
	else
	{
		Forward.Normalize();
	}

	FVector Right = CachedWorldAxis.Cross(Forward).Normalized();
	FVector Up = CachedWorldAxis;

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(CachedScale)
		* RotMatrix * FMatrix::MakeTranslationMatrix(CachedLocation);

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
	MarkPerObjectCBDirty();
}
