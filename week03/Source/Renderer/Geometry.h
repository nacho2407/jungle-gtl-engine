#pragma once

#include "EngineCore/EngineTypes.h"
#include "Math/Vector.h"
#include "EngineCore/Color.h"
#include "Renderer/TexCoord.h"
#include <d3d11.h>
#include "EngineCore/EngineTypes.h"

struct FGeometry
{
	TComPtr<ID3D11Buffer> VertexBuffer = nullptr;
	TComPtr<ID3D11Buffer> IndexBuffer = nullptr;
	TArray<FVector> Vertices;
	TArray<uint32_t> Indices;
	TArray<FColor> Colors;
	TArray<FTexCoord> TexCoords;


	bool IsVerticisValid() { return Vertices.empty(); }
	bool IsColorValid() { return Colors.empty(); }
	bool IsUVValid() { return TexCoords.empty(); }

	FGeometry() {}
	~FGeometry()
	{
		VertexBuffer.Reset();
		IndexBuffer.Reset();
	}

	void Bind(ID3D11DeviceContext* Context)
	{
		if (VertexBuffer == nullptr)
			return;
		Context->IASetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &VertexStride, &Offset);
		
		if (IndexBuffer != nullptr)
		{
			Context->IASetIndexBuffer(IndexBuffer.Get(), IndexFormat, 0);
		}
	}
	UINT VertexCount = 0;
	UINT VertexStride = 0;
	UINT Offset = 0;
	UINT IndexCount = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_UNKNOWN;
};