#pragma once

#include "EngineCore/EngineTypes.h"
#include "Math/Vector.h"
#include <d3d11.h>
#include "EngineCore/EngineTypes.h"

struct FGeometry
{
	TComPtr<ID3D11Buffer> VertexBuffer = nullptr;
	//TComPtr<ID3D11Buffer> IndexBuffer = nullptr;
	TArray<FVector> Vertices;
	UINT VertexCount = 0;
	UINT VertexStride = 0;
	UINT Offset = 0;
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
};