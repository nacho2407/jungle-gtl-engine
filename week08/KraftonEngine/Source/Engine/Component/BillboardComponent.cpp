#include "BillboardComponent.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Pipeline/BillboardSceneProxy.h"

DEFINE_CLASS(UBillboardComponent, UPrimitiveComponent)

FPrimitiveSceneProxy* UBillboardComponent::CreateSceneProxy()
{
	return new FBillboardSceneProxy(this);
}

void UBillboardComponent::CollectSelection(FRenderBus& Bus) const
{
	FMeshBuffer* Buffer = GetMeshBuffer();
	if (!Buffer || !Buffer->IsValid()) return;
	if (!SupportsOutline()) return;

	// Bus 카메라 벡터로 per-view 빌보드 행렬 계산 (다중 뷰포트 대응)
	FVector BillboardForward = Bus.GetCameraForward() * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Bus.GetCameraRight() * -1.0f, Bus.GetCameraUp());
	FMatrix PerViewBillboard = FMatrix::MakeScaleMatrix(GetWorldScale())
		* RotMatrix * FMatrix::MakeTranslationMatrix(GetWorldLocation());

	// SelectionMask: 스텐실에 선택 오브젝트 마킹
	FRenderCommand MaskCmd = {};
	MaskCmd.MeshBuffer = Buffer;
	MaskCmd.PerObjectConstants = FPerObjectConstants{ PerViewBillboard };
	MaskCmd.Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
	Bus.AddCommand(ERenderPass::SelectionMask, MaskCmd);
	// Billboard 계열은 AABB 제외
}

void UBillboardComponent::TickComponent(float DeltaTime)
{
	if (!GetOwner() || !GetOwner()->GetWorld()) return;

	const UCameraComponent* ActiveCamera = GetOwner()->GetWorld()->GetActiveCamera();
	if (!ActiveCamera) return;

	FVector WorldLocation = GetWorldLocation();
	FVector CameraForward = ActiveCamera->GetForwardVector().Normalized();
	FVector Forward = CameraForward * -1;
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f); // 임시 Up축 변경
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	CachedWorldMatrix = FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(WorldLocation);

	UpdateWorldAABB();
}

FMatrix UBillboardComponent::ComputeBillboardMatrix(const FVector& CameraForward) const
{
	// TickComponent와 동일한 로직
	FVector Forward = (CameraForward * -1.0f).Normalized();
	FVector WorldUp = FVector(0.0f, 0.0f, 1.0f);

	if (std::abs(Forward.Dot(WorldUp)) > 0.99f)
	{
		WorldUp = FVector(0.0f, 1.0f, 0.0f);
	}

	FVector Right = WorldUp.Cross(Forward).Normalized();
	FVector Up = Forward.Cross(Right).Normalized();

	FMatrix RotMatrix;
	RotMatrix.SetAxes(Forward, Right, Up);

	return FMatrix::MakeScaleMatrix(GetWorldScale()) * RotMatrix * FMatrix::MakeTranslationMatrix(GetWorldLocation());
}