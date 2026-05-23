#pragma once

#include "Core/Types/CoreTypes.h"
#include "Mesh/Static/StaticMesh.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"

#include "Particles/Runtime/ParticleRuntimeTypes.h"
#include "Particles/Module/ParticleModuleTypeDataBase.h"

struct FMeshParticleTransform
{
	FVector Position = FVector::ZeroVector;
	FVector Scale = FVector::OneVector;
	FVector RotationEuler = FVector::ZeroVector;
};

class FMeshParticleGeometry
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	void Clear();

	void AddMeshParticle(const FBaseParticle& Particle, const FMeshParticleTransform& Transform,
		const FStaticMeshSection& Section, const TArray<FNormalVertex>& SourceVertices, const TArray<uint32>& SourceIndices);

	bool Upload(ID3D11Device* Device, ID3D11DeviceContext* Context);

	ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer.GetBuffer(); }
	ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer.GetBuffer(); }
	uint32 GetVertexStride() const { return VertexStride; }
	uint32 GetIndexCount() const { return IndexCount; }

private:
	TArray<FVertexPNCTT> Vertices;
	TArray<uint32> Indices;

	FDynamicVertexBuffer VertexBuffer;
	FDynamicIndexBuffer IndexBuffer;

	uint32 VertexStride = sizeof(FVertexPNCTT);
	uint32 IndexCount = 0;
};
