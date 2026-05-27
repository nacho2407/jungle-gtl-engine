#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Command/DrawCommand.h"
#include "Materials/Material.h"
#include "Materials/MaterialManager.h"
#include "Object/Reflection/ObjectFactory.h"

// ============================================================
// FPrimitiveSceneProxy — 기본 구현
// ============================================================
FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
	: Owner(InComponent)
{
	if (!Owner->SupportsOutline())
		ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
}

FPrimitiveSceneProxy::~FPrimitiveSceneProxy() noexcept
{
	if (DefaultMaterial)
	{
		FMaterialManager::Get().DestroyTransientMaterial(DefaultMaterial);
		DefaultMaterial = nullptr;
	}
}

ERenderPass FPrimitiveSceneProxy::GetRenderPass() const
{
	if (!SectionDraws.empty() && SectionDraws[0].Material)
		return SectionDraws[0].Material->GetRenderPass();
	return ERenderPass::Opaque;
}

FShader* FPrimitiveSceneProxy::GetShader() const
{
	if (!SectionDraws.empty() && SectionDraws[0].Material)
		return SectionDraws[0].Material->GetShader();
	return nullptr;
}

void FPrimitiveSceneProxy::UpdateTransform()
{
	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(Owner->GetWorldMatrix());
	CachedWorldPos = PerObjectConstants.Model.GetLocation();
	CachedBounds = Owner->GetWorldBoundingBox();
	LastLODUpdateFrame = UINT32_MAX;
	MarkPerObjectCBDirty();
}

void FPrimitiveSceneProxy::UpdateMaterial()
{
	// 기본 PrimitiveComponent는 섹션별 머티리얼이 없음 — 서브클래스에서 오버라이드
}

void FPrimitiveSceneProxy::UpdateVisibility()
{
	bVisible = Owner->IsVisible();
	if (bVisible)
	{
		AActor* OwnerActor = Owner->GetOwner();
		if (OwnerActor && !OwnerActor->IsVisible())
			bVisible = false;
	}
	bCastShadow = Owner->GetCastShadow();
	bCastShadowAsTwoSided = Owner->GetCastShadowAsTwoSided();

	TranslucentSortPriority = Owner->GetTranslucentSortPriority();
}

void FPrimitiveSceneProxy::UpdateMesh()
{
	MeshBuffer = Owner->GetMeshBuffer();

	if (!DefaultMaterial)
	{
		DefaultMaterial = FMaterialManager::Get().CreateTransientMaterial(
			ERenderPass::Opaque, EBlendState::Opaque,
			EDepthStencilState::Default, ERasterizerState::SolidBackCull,
			FShaderManager::Get().GetOrCreate(EShaderPath::Primitive));
	}

	SectionDraws.clear();
	if (MeshBuffer && DefaultMaterial)
	{
		uint32 IdxCount = MeshBuffer->GetIndexBuffer().GetIndexCount();
		SectionDraws.push_back({ DefaultMaterial, 0, IdxCount });
	}
}

bool FPrimitiveSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	FMeshBuffer* Mesh = GetMeshBuffer();
	if (!Mesh || !Mesh->IsValid()) return false;

	OutBuffer = {};
	OutBuffer.VB = Mesh->GetVertexBuffer().GetBuffer();
	OutBuffer.VBStride = Mesh->GetVertexBuffer().GetStride();
	OutBuffer.IB = Mesh->GetIndexBuffer().GetBuffer();
	return OutBuffer.VB != nullptr;
}
