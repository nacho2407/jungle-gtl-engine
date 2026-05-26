#pragma once

#include "Core/Types/CoreTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"

#include "Particles/Runtime/ParticleRuntimeTypes.h"

// FSpriteParticleGeometry - 간단한 테스트 (Instancing 이전에 파티클 렌더링 테스트를 위한 기하 구조체)
class FSpriteParticleGeometry
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	void Clear();

	void AddParticleQuad(const FBaseParticle& Particle, const FVector& CameraRight, const FVector& CameraUp);
	void AddParticleQuad(const FBaseParticle& Particle, const FVector& CameraRight, const FVector& CameraUp,
		const FVector2& TopLeftUV, const FVector2& TopRightUV, const FVector2& BottomLeftUV, const FVector2& BottomRightUV);
	void AddQuad(const FVector& TopLeft, const FVector& TopRight, const FVector& BottomLeft, const FVector& BottomRight,
		const FVector4& Color, const FVector2& TopLeftUV, const FVector2& TopRightUV, const FVector2& BottomLeftUV, const FVector2& BottomRightUV);
	void AddQuad(const FVector& TopLeft, const FVector& TopRight, const FVector& BottomLeft, const FVector& BottomRight,
		const FVector4& TopLeftColor, const FVector4& TopRightColor, const FVector4& BottomLeftColor, const FVector4& BottomRightColor,
		const FVector2& TopLeftUV, const FVector2& TopRightUV, const FVector2& BottomLeftUV, const FVector2& BottomRightUV);

	uint32 AddVertex(const FParticleSpriteVertex& Vertex);
	void AddTriangle(uint32 A, uint32 B, uint32 C);

	bool Upload(ID3D11DeviceContext* Context);

	ID3D11Buffer* GetVertexBuffer() const { return VertexBuffer.GetBuffer(); }
	ID3D11Buffer* GetIndexBuffer() const { return IndexBuffer.GetBuffer(); }
	uint32 GetVertexStride() const { return VertexStride; }
	uint32 GetIndexCount() const { return IndexCount; }

private:
	TArray<FParticleSpriteVertex> Vertices;
	TArray<uint32> Indices;

	FDynamicVertexBuffer VertexBuffer;
	FDynamicIndexBuffer IndexBuffer;
	uint32 VertexStride = sizeof(FParticleSpriteVertex);
	uint32 IndexCount = 0;
};
