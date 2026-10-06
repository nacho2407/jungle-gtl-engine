#include "Render/Proxy/PrimitiveSceneProxy.h"
#include "Component/PrimitiveComponent.h"
#include "GameFramework/AActor.h"
#include "Render/Shader/ShaderManager.h"
#include "Render/Command/DrawCommand.h"
#include "Materials/Material.h"
#include "Object/Reflection/ObjectFactory.h"
#include "Object/GarbageCollection.h"

// ============================================================
// FPrimitiveSceneProxy — 기본 구현
// ============================================================
FPrimitiveSceneProxy::FPrimitiveSceneProxy(UPrimitiveComponent* InComponent)
	: Owner(InComponent)
{
	UPrimitiveComponent* OwnerComponent = GetOwner();
	if (OwnerComponent && !OwnerComponent->SupportsOutline())
		ProxyFlags &= ~EPrimitiveProxyFlags::SupportsOutline;
}

FPrimitiveSceneProxy::~FPrimitiveSceneProxy() noexcept
{
	if (DefaultMaterial)
	{
		UObjectManager::Get().DestroyObject(DefaultMaterial);
		DefaultMaterial = nullptr;
	}
}

void FPrimitiveSceneProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(DefaultMaterial);
	for (const FMeshSectionDraw& Draw : SectionDraws)
	{
		Collector.AddReferencedObject(Draw.Material);
	}
}

bool FPrimitiveSceneProxy::HasValidOwner() const
{
	return GetOwner() != nullptr;
}

ERenderPass FPrimitiveSceneProxy::GetRenderPass() const
{
	if (!SectionDraws.empty() && IsValid(SectionDraws[0].Material))
		return SectionDraws[0].Material->GetRenderPass();
	return ERenderPass::Opaque;
}

FShader* FPrimitiveSceneProxy::GetShader() const
{
	if (!SectionDraws.empty() && IsValid(SectionDraws[0].Material))
		return SectionDraws[0].Material->GetShader();
	return nullptr;
}

void FPrimitiveSceneProxy::UpdateTransform()
{
	UPrimitiveComponent* OwnerComponent = GetOwner();
	if (!OwnerComponent)
	{
		bVisible = false;
		return;
	}

	PerObjectConstants = FPerObjectConstants::FromWorldMatrix(OwnerComponent->GetWorldMatrix());
	CachedWorldPos = PerObjectConstants.Model.GetLocation();
	CachedBounds = OwnerComponent->GetWorldBoundingBox();
	LastLODUpdateFrame = UINT32_MAX;
	MarkPerObjectCBDirty();
}

void FPrimitiveSceneProxy::UpdateMaterial()
{
	// 기본 PrimitiveComponent는 섹션별 머티리얼이 없음 — 서브클래스에서 오버라이드
}

void FPrimitiveSceneProxy::UpdateVisibility()
{
	UPrimitiveComponent* OwnerComponent = GetOwner();
	if (!OwnerComponent)
	{
		bVisible = false;
		return;
	}

	bVisible = OwnerComponent->IsVisible();
	if (bVisible)
	{
		AActor* OwnerActor = OwnerComponent->GetOwner();
		if (!IsValid(OwnerActor) || !OwnerActor->IsVisible())
			bVisible = false;
	}
	bCastShadow = OwnerComponent->GetCastShadow();
	bCastShadowAsTwoSided = OwnerComponent->GetCastShadowAsTwoSided();
}

void FPrimitiveSceneProxy::UpdateMesh()
{
	UPrimitiveComponent* OwnerComponent = GetOwner();
	if (!OwnerComponent)
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();
		bVisible = false;
		return;
	}

	MeshBuffer = OwnerComponent->GetMeshBuffer();

	if (!DefaultMaterial)
	{
		DefaultMaterial = UMaterial::CreateTransient(
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
