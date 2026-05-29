#pragma once

#include "Render/Geometry/DebugGeometryTypes.h"
#include "Render/Resource/Buffer.h"
#include "Render/Types/VertexTypes.h"

class FPhysicsDebugSolidGeometry
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();
	void Clear();

	void SetMesh(const FPhysicsDebugSolidMesh& InMesh);
	void AppendMesh(const FPhysicsDebugSolidMesh& InMesh);
	bool UploadBuffers(ID3D11DeviceContext* Context);

	ID3D11Buffer* GetVBBuffer() const { return VB.GetBuffer(); }
	ID3D11Buffer* GetIBBuffer() const { return IB.GetBuffer(); }
	uint32 GetVBStride() const { return VB.GetStride(); }
	uint32 GetIndexCount() const { return static_cast<uint32>(Mesh.Indices.size()); }

private:
	FPhysicsDebugSolidMesh Mesh;
	TArray<FVertex> DrawVertices;
	FDynamicVertexBuffer VB;
	FDynamicIndexBuffer IB;
	ID3D11Device* Device = nullptr;
};
