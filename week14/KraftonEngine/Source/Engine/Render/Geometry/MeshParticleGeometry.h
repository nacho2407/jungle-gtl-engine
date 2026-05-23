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

struct FMeshParticleInstanceData
{
	FVector4 WorldRow0;
	FVector4 WorldRow1;
	FVector4 WorldRow2;
	FVector4 WorldRow3;
	FVector4 Color;
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

inline FMeshParticleInstanceData MakeMeshParticleInstanceData(const FMeshParticleTransform& Transform, const FVector4& Color)
{
	const FMatrix ScaleMatrix = FMatrix::MakeScaleMatrix(Transform.Scale);
	const FMatrix RotationMatrix = FMatrix::MakeRotationEuler(Transform.RotationEuler);
	const FMatrix TranslationMatrix = FMatrix::MakeTranslationMatrix(Transform.Position);

	const FMatrix WorldMatrix = ScaleMatrix * RotationMatrix * TranslationMatrix;

	FMeshParticleInstanceData Data;
	Data.WorldRow0 = FVector4(WorldMatrix.M[0][0], WorldMatrix.M[0][1], WorldMatrix.M[0][2], WorldMatrix.M[0][3]);
	Data.WorldRow1 = FVector4(WorldMatrix.M[1][0], WorldMatrix.M[1][1], WorldMatrix.M[1][2], WorldMatrix.M[1][3]);
	Data.WorldRow2 = FVector4(WorldMatrix.M[2][0], WorldMatrix.M[2][1], WorldMatrix.M[2][2], WorldMatrix.M[2][3]);
	Data.WorldRow3 = FVector4(WorldMatrix.M[3][0], WorldMatrix.M[3][1], WorldMatrix.M[3][2], WorldMatrix.M[3][3]);
	Data.Color = Color;
	return Data;
}
