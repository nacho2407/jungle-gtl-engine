#include "Render/Geometry/MeshParticleGeometry.h"

void FMeshParticleGeometry::Create(ID3D11Device* InDevice)
{
	VertexBuffer.Create(InDevice, 1024, VertexStride);
	IndexBuffer.Create(InDevice, 1024 * 3);
}

void FMeshParticleGeometry::Release()
{
	VertexBuffer.Release();
	IndexBuffer.Release();
}

void FMeshParticleGeometry::Clear()
{
	Vertices.clear();
	Indices.clear();
	IndexCount = 0;
}

void FMeshParticleGeometry::AddMeshParticle(const FBaseParticle& Particle, const FMeshParticleTransform& Transform,
	const FStaticMeshSection& Section, const TArray<FNormalVertex>& SourceVertices, const TArray<uint32>& SourceIndices)
{
	const uint32 FirstIndex = Section.FirstIndex;
	const uint32 SourceIndexCount = Section.NumTriangles * 3;

	if (SourceIndexCount == 0)
	{
		return;
	}

	if (FirstIndex + SourceIndexCount > static_cast<uint32>(SourceIndices.size()))
	{
		return;
	}

	const FMatrix RotationMatrix = FMatrix::MakeRotationEuler(Transform.RotationEuler);

	for (uint32 i = 0; i < SourceIndexCount; ++i)
	{
		const uint32 SourceIndex = SourceIndices[FirstIndex + i];
		if (SourceIndex >= static_cast<uint32>(SourceVertices.size())) continue;

		const FNormalVertex& Src = SourceVertices[SourceIndex];
		FVector SourceTangent = FVector(Src.tangent.X, Src.tangent.Y, Src.tangent.Z);

		const FVector ScaledPosition(
			Src.pos.X * Transform.Scale.X,
			Src.pos.Y * Transform.Scale.Y,
			Src.pos.Z * Transform.Scale.Z);

		FVertexPNCTT Dst;
		Dst.Position = Transform.Position + RotationMatrix.TransformVector(ScaledPosition);
		Dst.Normal = RotationMatrix.TransformVector(Src.normal).Normalized();
		Dst.Tangent = RotationMatrix.TransformVector(SourceTangent).Normalized();
		Dst.Color = Particle.Color;
		Dst.UV = Src.tex;

		const uint32 NewVertexIndex = static_cast<uint32>(Vertices.size());
		Vertices.push_back(Dst);
		Indices.push_back(NewVertexIndex);
	}

	IndexCount = static_cast<uint32>(Indices.size());
}

bool FMeshParticleGeometry::Upload(ID3D11Device* Device, ID3D11DeviceContext* Context)
{
	if (Vertices.empty() || Indices.empty())
	{
		return false;
	}

	VertexBuffer.EnsureCapacity(Device, static_cast<uint32>(Vertices.size()));
	IndexBuffer.EnsureCapacity(Device, static_cast<uint32>(Indices.size()));

	if (!VertexBuffer.Update(Context, Vertices.data(), static_cast<uint32>(Vertices.size())))
	{
		return false;
	}
	if (!IndexBuffer.Update(Context, Indices.data(), static_cast<uint32>(Indices.size())))
	{
		return false;
	}
	return true;
}
