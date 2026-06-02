#include "Render/Proxy/ClothSceneProxy.h"

#include "Component/Primitive/ClothComponent.h"
#include "Materials/MaterialManager.h"
#include "Materials/Material.h"
#include "Render/Command/DrawCommand.h"
#include "Render/Shader/ShaderManager.h"

FClothSceneProxy::FClothSceneProxy(UClothComponent* InComponent)
	: FPrimitiveSceneProxy(InComponent)
{
}

UClothComponent* FClothSceneProxy::GetClothComponent() const
{
	return static_cast<UClothComponent*>(GetOwner());
}

void FClothSceneProxy::UpdateMaterial()
{
	const UClothComponent* ClothComp = GetClothComponent();
	if (!ClothComp)
	{
		SectionDraws.clear();
		return;
	}

	UMaterialInterface* Material = ClothComp->GetMaterial();

	if (!Material)
	{
		if (!DefaultMaterial)
		{
			DefaultMaterial = FMaterialManager::Get().GetOrCreateMaterial("Content/Material/Editor/ClothDefault.mat");
		}

		Material = DefaultMaterial;
	}

	const TArray<uint32>& Indices = ClothComp->GetClothInstance().GetRenderIndices();

	SectionDraws.clear();
	if (!Indices.empty() && Material)
	{
		FMeshSectionDraw Draw;
		Draw.Material = Material;
		Draw.FirstIndex = 0;
		Draw.IndexCount = static_cast<uint32>(Indices.size());
		SectionDraws.push_back(Draw);
	}
}

void FClothSceneProxy::UpdateMesh()
{
	UpdateMaterial();

	bBufferNeedsCreate = true;
	UploadedRevision = 0;
}

bool FClothSceneProxy::PrepareDrawBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context, FDrawCommandBuffer& OutBuffer) const
{
	const UClothComponent* ClothComp = GetClothComponent();
	if (!Device || !Context || !ClothComp) return false;

	const FClothInstance& Cloth = ClothComp->GetClothInstance();
	const TArray<FVertexPNCTT>& Vertices = Cloth.GetRenderVertices();
	const TArray<uint32>& Indices = Cloth.GetRenderIndices();

	if (Vertices.empty() || Indices.empty()) return false;

	const uint32 VertexCount = static_cast<uint32>(Vertices.size());
	const uint32 IndexCount = static_cast<uint32>(Indices.size());
	
	if (bBufferNeedsCreate || !DynamicVertexBuffer.GetBuffer() || !DynamicIndexBuffer.GetBuffer())
	{
		DynamicVertexBuffer.Create(Device, VertexCount, sizeof(FVertexPNCTT));
		DynamicIndexBuffer.Create(Device, IndexCount);
		bBufferNeedsCreate = false;
	}
	
	DynamicVertexBuffer.EnsureCapacity(Device, VertexCount);
	DynamicIndexBuffer.EnsureCapacity(Device, IndexCount);

	const uint64 CurrentRevision = Cloth.GetRenderRevision();
	if (UploadedRevision != CurrentRevision)
	{
		if (!DynamicVertexBuffer.Update(Context, Vertices.data(), VertexCount)) return false;
		if (!DynamicIndexBuffer.Update(Context, Indices.data(), IndexCount)) return false;
		UploadedRevision = CurrentRevision;
	}

	OutBuffer = {};
	OutBuffer.VB = DynamicVertexBuffer.GetBuffer();
	OutBuffer.VBStride = DynamicVertexBuffer.GetStride();
	OutBuffer.IB = DynamicIndexBuffer.GetBuffer();
	OutBuffer.IndexCount = IndexCount;

	return OutBuffer.VB && OutBuffer.IB;
}
