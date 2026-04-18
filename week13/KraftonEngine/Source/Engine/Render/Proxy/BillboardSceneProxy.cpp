#include "Render/Proxy/BillboardSceneProxy.h"
#include "Component/BillboardComponent.h"
#include "Render/Resource/ShaderManager.h"
#include "Render/Resource/MeshBufferManager.h"
#include "Render/Pipeline/FrameContext.h"
#include "GameFramework/AActor.h"
#include "Materials/Material.h"
#include "Texture/Texture2D.h"

// ============================================================
// FBillboardSceneProxy
// ============================================================
FBillboardSceneProxy::FBillboardSceneProxy(UBillboardComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	bPerViewportUpdate = true;
	bShowAABB = false;
}

UBillboardComponent* FBillboardSceneProxy::GetBillboardComponent() const
{
	return static_cast<UBillboardComponent*>(Owner);
}

// ============================================================
// UpdateMesh — TexturedQuad + Material shader/states
// ============================================================
void FBillboardSceneProxy::UpdateMesh()
{
	UBillboardComponent* Comp = GetBillboardComponent();
	UMaterial* Mat = Comp ? Comp->GetMaterial() : nullptr;

	if (Mat)
	{
		// TexturedQuad (FVertexPNCT with UVs)
		MeshBuffer = &FMeshBufferManager::Get().GetMeshBuffer(EMeshShape::TexturedQuad);
		
		Shader = Mat->GetShader();
		if (!Shader)
		{
			Shader = FShaderManager::Get().GetShader(EShaderType::Billboard);
		}

		Pass = Mat->GetRenderPass();
		Material = Mat;

		UTexture2D* DiffuseTex = nullptr;
		if (Mat->GetTextureParameter("DiffuseTexture", DiffuseTex))
		{
			DiffuseSRV = DiffuseTex->GetSRV();
		}
		else
		{
			DiffuseSRV = nullptr;
		}
	}
	else
	{
		MeshBuffer = Owner->GetMeshBuffer();
		Shader = FShaderManager::Get().GetShader(EShaderType::Primitive);
		Pass = ERenderPass::Opaque;
		DiffuseSRV = nullptr;
		Material = nullptr;
	}
}

// ============================================================
// UpdatePerViewport — 뷰포트 카메라 기반 빌보드 행렬 갱신
// ============================================================
void FBillboardSceneProxy::UpdatePerViewport(const FFrameContext& Frame)
{
	UBillboardComponent* Comp = GetBillboardComponent();
	bVisible = Comp->IsVisible();
	if (!bVisible) return;

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

	// Frame 카메라 벡터로 per-view 빌보드 행렬 계산
	FVector BillboardForward = Frame.CameraForward * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Frame.CameraRight, Frame.CameraUp);
	FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(Comp->GetWorldScale())
		* RotMatrix * FMatrix::MakeTranslationMatrix(Comp->GetWorldLocation());

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
	MarkPerObjectCBDirty();
}
