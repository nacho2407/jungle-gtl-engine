#pragma once

#include "EngineCore/EngineTypes.h"
#include "Math/Vector.h"
#include "EngineCore/Color.h"
#include "Renderer/UV.h"
#include <d3d11.h>
#include "EngineCore/EngineTypes.h"

struct FGeometry
{
	TComPtr<ID3D11Buffer> VertexBuffer = nullptr;
	TComPtr<ID3D11Buffer> IndexBuffer = nullptr;
	TArray<FVector> Vertices;
	TArray<FColor> Colors;
	TArray<UV> UVs;

	bool IsVerticisValid() { return Vertices.empty(); }
	bool IsColorValid() { return Colors.empty(); }
	bool IsUVValid() { return UVs.empty(); }

	FGeometry() {}
	~FGeometry()
	{
		VertexBuffer.Reset();
	}

	void Bind(ID3D11DeviceContext* Context)
	{
		if (VertexBuffer == nullptr)
			return;
		Context->IASetVertexBuffers(0, 1, VertexBuffer.GetAddressOf(), &VertexStride, &Offset);
		//Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	}
	UINT VertexCount = 0;
	UINT VertexStride = 0;
	UINT Offset = 0;
};