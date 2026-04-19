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
	ProxyFlags |= EPrimitiveProxyFlags::PerViewportUpdate;
	ProxyFlags &= ~EPrimitiveProxyFlags::ShowAABB;

	if (InComponent->IsEditorOnly())
		ProxyFlags |= EPrimitiveProxyFlags::EditorOnly;
}

UBillboardComponent* FBillboardSceneProxy::GetBillboardComponent() const
{
	return static_cast<UBillboardComponent*>(GetOwner());
}

// ============================================================
// UpdateTransform — Scale/Location 캐싱
// ============================================================
void FBillboardSceneProxy::UpdateTransform()
{
	FPrimitiveSceneProxy::UpdateTransform();
	UBillboardComponent* Comp = GetBillboardComponent();
	CachedScale = Comp->GetWorldScale();
	CachedLocation = Comp->GetWorldLocation();
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
		MeshBuffer = GetOwner()->GetMeshBuffer();
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
	if (!bVisible) return;

	// Frame 카메라 벡터로 per-view 빌보드 행렬 계산
	FVector BillboardForward = Frame.CameraForward * -1.0f;
	FMatrix RotMatrix;
	RotMatrix.SetAxes(BillboardForward, Frame.CameraRight, Frame.CameraUp);
	FMatrix BillboardMatrix = FMatrix::MakeScaleMatrix(CachedScale)
		* RotMatrix * FMatrix::MakeTranslationMatrix(CachedLocation);

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(BillboardMatrix);
	MarkPerObjectCBDirty();
}
