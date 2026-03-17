#pragma once

#include "d3d11.h"
#include "EngineTypes.h"
#include "Vector.h"

struct FGeometry
{
	ID3D11Buffer* VertexBuffer = nullptr;
	TArray<FVector> Vertices;
	UINT VertexCount = 0;
	UINT VertexStride = 0;
	UINT Offset = 0;
	FGeometry() {}
	~FGeometry() {}

	void BindBuffers(ID3D11DeviceContext* Context)
	{
		Context->IASetVertexBuffers(0, 1, &VertexBuffer, &VertexStride, &Offset);
		//Context->IASetIndexBuffer(IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	}
};