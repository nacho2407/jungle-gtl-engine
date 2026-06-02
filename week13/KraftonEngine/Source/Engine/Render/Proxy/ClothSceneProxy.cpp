#include "Render/Proxy/ClothSceneProxy.h"

#include "Cloth/ClothTypes.h"
#include "Component/Primitive/ClothComponent.h"
#include "Materials/MaterialManager.h"
#include "Object/GarbageCollection.h"
#include "Object/Object.h"
#include "Profiling/Stats/ClothStats.h"
#include "Render/Command/DrawCommand.h"

#include <algorithm>

FClothSceneProxy::FClothSceneProxy(UClothComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
	ProxyFlags |= EPrimitiveProxyFlags::Cloth;
}

UClothComponent* FClothSceneProxy::GetClothComponent() const
{
	return Cast<UClothComponent>(GetOwner());
}

void FClothSceneProxy::UpdateMaterial()
{
	RebuildSectionDraws();
}

void FClothSceneProxy::UpdateMesh()
{
	if (!HasValidOwner())
	{
		MeshBuffer = nullptr;
		SectionDraws.clear();
		CachedMaterial = nullptr;
		CachedVertexCount = 0;
		CachedIndexCount = 0;
		bVisible = false;
		return;
	}

	// Cloth는 FMeshBuffer 대신 component의 CPU render data와 proxy dynamic buffer를 사용
	MeshBuffer = nullptr;

	UClothComponent* ClothComponent = GetClothComponent();
	if (!IsValid(ClothComponent))
	{
		SectionDraws.clear();
		CachedMaterial = nullptr;
		CachedVertexCount = 0;
		CachedIndexCount = 0;
		bVisible = false;
		return;
	}

	// 초기 등록 또는 property dirty 직후 render data가 비어 있으면 한 번 생성 보장
	ClothComponent->RebuildClothIfNeeded(false);

	const FClothRenderData& RenderData = ClothComponent->GetClothRenderData();
	CachedVertexCount = static_cast<uint32>(RenderData.Vertices.size());
	CachedIndexCount = static_cast<uint32>(RenderData.Indices.size());
	UploadedRevision = 0;
	bDynamicBuffersNeedCreate = true;

	RebuildSectionDraws();
}

bool FClothSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	UClothComponent* ClothComponent = GetClothComponent();
	if (!IsValid(ClothComponent))
	{
		return false;
	}

	// editor 초기 표시 경로에서 tick보다 draw 준비가 먼저 오는 경우 방어
	ClothComponent->RebuildClothIfNeeded(false);

	const FClothRenderData& RenderData = ClothComponent->GetClothRenderData();
	const uint32 VertexCount = static_cast<uint32>(RenderData.Vertices.size());
	const uint32 IndexCount = static_cast<uint32>(RenderData.Indices.size());
	if (!RenderData.IsValid() || VertexCount == 0 || IndexCount == 0)
	{
		return false;
	}

	if (bDynamicBuffersNeedCreate || !DynamicVertexBuffer.GetBuffer() || !DynamicIndexBuffer.GetBuffer())
	{
		const uint32 InitialVertexCapacity = (std::max)(CachedVertexCount, VertexCount);
		const uint32 InitialIndexCapacity = (std::max)(CachedIndexCount, IndexCount);
		DynamicVertexBuffer.Create(Device, InitialVertexCapacity, sizeof(FVertexPNCTT));
		DynamicIndexBuffer.Create(Device, InitialIndexCapacity);
		UploadedRevision = 0;
		bDynamicBuffersNeedCreate = false;
	}

	DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);
	DynamicIndexBuffer.EnsureCapacity(Device, IndexCount);

	const uint64 CurrentRevision = RenderData.Revision;
	if (UploadedRevision != CurrentRevision)
	{
		if (!DynamicVertexBuffer.Update(Context, RenderData.Vertices.data(), VertexCount))
		{
			return false;
		}

		if (!DynamicIndexBuffer.Update(Context, RenderData.Indices.data(), IndexCount))
		{
			return false;
		}

		// revision 변경으로 실제 vertex upload가 성공한 frame만 stat에 반영
		CLOTH_STATS_ADD_VERTEX_UPLOAD();
		UploadedRevision = CurrentRevision;
	}

	OutBuffer = {};
	OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
	OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
	OutBuffer.IB = DynamicIndexBuffer.GetBuffer();
	return OutBuffer.VB != nullptr && OutBuffer.IB != nullptr;
}

void FClothSceneProxy::AddReferencedObjects(FReferenceCollector& Collector)
{
	FPrimitiveSceneProxy::AddReferencedObjects(Collector);
	Collector.AddReferencedObject(CachedMaterial, "FClothSceneProxy.CachedMaterial");
}

void FClothSceneProxy::RebuildSectionDraws()
{
	SectionDraws.clear();
	CachedMaterial = nullptr;

	UClothComponent* ClothComponent = GetClothComponent();
	if (!IsValid(ClothComponent))
	{
		MeshBuffer = nullptr;
		return;
	}

	const FClothRenderData& RenderData = ClothComponent->GetClothRenderData();
	if (!RenderData.IsValid())
	{
		return;
	}

	CachedMaterial = ClothComponent->GetMaterial(0);
	if (!CachedMaterial)
	{
		CachedMaterial = FMaterialManager::Get().GetOrCreateMaterial("None");
	}

	SectionDraws.reserve(RenderData.Sections.size());
	for (const FClothRenderSection& Section : RenderData.Sections)
	{
		if (Section.IndexCount == 0)
		{
			continue;
		}

		FMeshSectionDraw Draw;
		Draw.Material = CachedMaterial;
		Draw.FirstIndex = Section.FirstIndex;
		Draw.IndexCount = Section.IndexCount;
		SectionDraws.push_back(Draw);
	}
}
