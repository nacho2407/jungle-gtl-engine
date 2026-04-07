#include "MeshData.h"
#include "Object/Class.h"
#include "Vertex.h"
#include "Level/MeshBVH.h"

bool FStaticMesh::UpdateVertexAndIndexBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context)
{
	if (!bIsDirty) return true;
	bIsDirty = false;
	return CreateVertexAndIndexBuffer(Device);
}

bool FStaticMesh::CreateVertexAndIndexBuffer(ID3D11Device* Device)
{
	Release();
	if (Vertices.empty() || Indices.empty()) return false;

	// Vertex Buffer
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VBDesc.ByteWidth = static_cast<UINT>(sizeof(FVertex) * Vertices.size());
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VBData = {};
	VBData.pSysMem = Vertices.data();

	HRESULT Hr = Device->CreateBuffer(&VBDesc, &VBData, &VertexBuffer);
	if (FAILED(Hr))
	{
		printf("[FMeshData] Failed to create vertex buffer\n");
		return false;
	}

	// Index Buffer
	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * Indices.size());
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IBData = {};
	IBData.pSysMem = Indices.data();

	Hr = Device->CreateBuffer(&IBDesc, &IBData, &IndexBuffer);
	if (FAILED(Hr))
	{
		printf("[FMeshData] Failed to create index buffer\n");
		VertexBuffer->Release();
		VertexBuffer = nullptr;
		return false;
	}

	return true;
}

bool FDynamicMesh::CreateVertexAndIndexBuffer(ID3D11Device* Device)
{
	Release();

	if (Vertices.empty() || Indices.empty()) return false;

	// 잦은 버퍼 재생성을 막기 위해 실제 필요한 크기보다 1.5배 크게(여유 공간) 할당
	MaxVertexCapacity = static_cast<uint32>(Vertices.size());
	MaxIndexCapacity = static_cast<uint32>(Indices.size());


	// 1. Vertex Buffer (DYNAMIC + CPU_ACCESS_WRITE)
	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.Usage = D3D11_USAGE_DYNAMIC;
	VBDesc.ByteWidth = sizeof(FVertex) * MaxVertexCapacity;
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	VBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA VBData = {};
	VBData.pSysMem = Vertices.data();

	HRESULT Hr = Device->CreateBuffer(&VBDesc, &VBData, &VertexBuffer);
	if (FAILED(Hr))
	{
		printf("[FDynamicMesh] Failed to create vertex buffer\n");
		return false;
	}

	// 2. Index Buffer (DYNAMIC + CPU_ACCESS_WRITE)
	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.Usage = D3D11_USAGE_DYNAMIC;
	IBDesc.ByteWidth = sizeof(uint32) * MaxIndexCapacity;
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	IBDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA IBData = {};
	IBData.pSysMem = Indices.data();

	Hr = Device->CreateBuffer(&IBDesc, &IBData, &IndexBuffer);
	if (FAILED(Hr))
	{
		printf("[FDynamicMesh] Failed to create index buffer\n");
		Release();
		return false;
	}

	return true;
}

bool FDynamicMesh::UpdateVertexAndIndexBuffer(ID3D11Device* Device, ID3D11DeviceContext* Context)
{
	if (!bIsDirty)
	{
		return true;
	}

	if (!VertexBuffer || !IndexBuffer ||
		Vertices.size() > MaxVertexCapacity ||
		Indices.size() > MaxIndexCapacity)
	{
		bIsDirty = false;
		return CreateVertexAndIndexBuffer(Device);
	}

	D3D11_MAPPED_SUBRESOURCE MappedVB;
	if (SUCCEEDED(Context->Map(VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedVB)))
	{
		memcpy(MappedVB.pData, Vertices.data(), sizeof(FVertex) * Vertices.size());
		Context->Unmap(VertexBuffer, 0);
	}

	D3D11_MAPPED_SUBRESOURCE MappedIB;
	if (SUCCEEDED(Context->Map(IndexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedIB)))
	{
		memcpy(MappedIB.pData, Indices.data(), sizeof(uint32) * Indices.size());
		Context->Unmap(IndexBuffer, 0);
	}

	bIsDirty = false;
	return true;
}

IMPLEMENT_RTTI(UStaticMesh, UObject)
const FString& UStaticMesh::GetAssetPathFileName() const
{
	if (StaticMeshAsset) return StaticMeshAsset->PathFileName;
	static FString EmptyPath = "";
	return EmptyPath;
}

void UStaticMesh::BuildAccelerationStructureIfNeeded() const
{
	if (TriangleBVH || !StaticMeshAsset)
	{
		return;
	}

	TriangleBVH = std::make_unique<FMeshBVH>();
	TriangleBVH->Build(*StaticMeshAsset);
}

void UStaticMesh::VisitMeshBVHNodes(const FBVHNodeVisitor& Visitor) const
{
	BuildAccelerationStructureIfNeeded();
	if (TriangleBVH && TriangleBVH->IsValid())
	{
		TriangleBVH->VisitNodes(Visitor);
	}
}

bool UStaticMesh::IntersectLocalRay(const FVector& RayOrigin, const FVector& RayDirection, float& OutDistance) const
{
	BuildAccelerationStructureIfNeeded();
	if (!TriangleBVH || !TriangleBVH->IsValid())
	{
		return false;
	}

	return TriangleBVH->IntersectRay(RayOrigin, RayDirection, OutDistance);
}