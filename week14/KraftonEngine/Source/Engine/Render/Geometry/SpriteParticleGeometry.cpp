#include "Render/Geometry/SpriteParticleGeometry.h"

void FSpriteParticleGeometry::Create(ID3D11Device* InDevice)
{
	VertexBuffer.Create(InDevice, 1024, VertexStride);
	IndexBuffer.Create(InDevice, 1024 * 6);
}

void FSpriteParticleGeometry::Release()
{
	VertexBuffer.Release();
	IndexBuffer.Release();
}

void FSpriteParticleGeometry::Clear()
{
	Vertices.clear();
	Indices.clear();
	IndexCount = 0;
}

void FSpriteParticleGeometry::AddParticleQuad(const FBaseParticle& Particle, const FVector& CameraRight, const FVector& CameraUp)
{
	const FVector Center = Particle.Position;
	const FVector HalfRight = CameraRight * (Particle.Size.X * 0.5f);
	const FVector HalfUp = CameraUp * (Particle.Size.Y * 0.5f);

	const FVector TopLeft = Center - HalfRight + HalfUp;
	const FVector TopRight = Center + HalfRight + HalfUp;
	const FVector BottomLeft = Center - HalfRight - HalfUp;
	const FVector BottomRight = Center + HalfRight - HalfUp;

	const uint32 StartIndex = static_cast<uint32>(Vertices.size());

	Vertices.push_back({ TopLeft, Particle.Color, FVector2(0.0f, 0.0f) });
	Vertices.push_back({ TopRight, Particle.Color, FVector2(1.0f, 0.0f) });
	Vertices.push_back({ BottomLeft, Particle.Color, FVector2(0.0f, 1.0f) });
	Vertices.push_back({ BottomRight, Particle.Color, FVector2(1.0f, 1.0f) });
	Indices.push_back(StartIndex + 0);
	Indices.push_back(StartIndex + 1);
	Indices.push_back(StartIndex + 2);
	Indices.push_back(StartIndex + 1);
	Indices.push_back(StartIndex + 3);
	Indices.push_back(StartIndex + 2);

	IndexCount += 6;
}

bool FSpriteParticleGeometry::Upload(ID3D11DeviceContext* Context)
{
	if (Vertices.empty() || Indices.empty())
	{
		return false;
	}
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
