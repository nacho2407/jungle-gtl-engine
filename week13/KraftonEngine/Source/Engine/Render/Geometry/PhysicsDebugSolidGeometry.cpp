#include "PhysicsDebugSolidGeometry.h"

void FPhysicsDebugSolidGeometry::Create(ID3D11Device* InDevice)
{
	Release();

	Device = InDevice;
	if (!Device)
	{
		return;
	}

	Device->AddRef();
	VB.Create(Device, 1024, sizeof(FVertex));
	IB.Create(Device, 3072);
}

void FPhysicsDebugSolidGeometry::Release()
{
	VB.Release();
	IB.Release();
	Mesh.Reset();
	DrawVertices.clear();

	if (Device)
	{
		Device->Release();
		Device = nullptr;
	}
}

void FPhysicsDebugSolidGeometry::Clear()
{
	Mesh.Reset();
	DrawVertices.clear();
}

void FPhysicsDebugSolidGeometry::SetMesh(const FPhysicsDebugSolidMesh& InMesh)
{
	Mesh = InMesh;
	DrawVertices.clear();
}

void FPhysicsDebugSolidGeometry::AppendMesh(const FPhysicsDebugSolidMesh& InMesh)
{
	if (!InMesh.IsValid())
	{
		return;
	}

	const uint32 BaseVertex = static_cast<uint32>(Mesh.Vertices.size());
	Mesh.Vertices.insert(Mesh.Vertices.end(), InMesh.Vertices.begin(), InMesh.Vertices.end());
	DrawVertices.clear();
	Mesh.Indices.reserve(Mesh.Indices.size() + InMesh.Indices.size());
	for (uint32 Index : InMesh.Indices)
	{
		Mesh.Indices.push_back(BaseVertex + Index);
	}
}

bool FPhysicsDebugSolidGeometry::UploadBuffers(ID3D11DeviceContext* Context)
{
	if (!Context || !Device || !Mesh.IsValid())
	{
		return false;
	}

	const uint32 VertexCount = static_cast<uint32>(Mesh.Vertices.size());
	const uint32 IndexCount = static_cast<uint32>(Mesh.Indices.size());
	DrawVertices.clear();
	DrawVertices.reserve(VertexCount);
	for (const FPhysicsDebugVertex& Vertex : Mesh.Vertices)
	{
		DrawVertices.push_back({ Vertex.Position, Vertex.Color, 0 });
	}

	VB.EnsureCapacity(Device, VertexCount);
	IB.EnsureCapacity(Device, IndexCount);

	if (!VB.Update(Context, DrawVertices.data(), VertexCount))
	{
		return false;
	}

	if (!IB.Update(Context, Mesh.Indices.data(), IndexCount))
	{
		return false;
	}

	return true;
}
