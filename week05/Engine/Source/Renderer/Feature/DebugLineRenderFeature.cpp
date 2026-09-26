#include "Renderer/Feature/DebugLineRenderFeature.h"

#include "Renderer/Renderer.h"
#include "Renderer/Vertex.h"

FDebugLineRenderFeature::~FDebugLineRenderFeature()
{
	Release();
}

void FDebugLineRenderFeature::AddLine(
	FDebugLineRenderRequest& Request,
	const FVector& Start,
	const FVector& End,
	const FVector4& Color)
{
	Request.Lines.push_back({ Start, End, Color });
}

void FDebugLineRenderFeature::AddCube(
	FDebugLineRenderRequest& Request,
	const FVector& Center,
	const FVector& BoxExtent,
	const FVector4& Color)
{
	const FVector Vertices[8] = {
		Center + FVector(-BoxExtent.X, -BoxExtent.Y, -BoxExtent.Z),
		Center + FVector(-BoxExtent.X, -BoxExtent.Y, BoxExtent.Z),
		Center + FVector(-BoxExtent.X, BoxExtent.Y, -BoxExtent.Z),
		Center + FVector(-BoxExtent.X, BoxExtent.Y, BoxExtent.Z),
		Center + FVector(BoxExtent.X, -BoxExtent.Y, -BoxExtent.Z),
		Center + FVector(BoxExtent.X, -BoxExtent.Y, BoxExtent.Z),
		Center + FVector(BoxExtent.X, BoxExtent.Y, -BoxExtent.Z),
		Center + FVector(BoxExtent.X, BoxExtent.Y, BoxExtent.Z)
	};

	AddLine(Request, Vertices[0], Vertices[4], Color);
	AddLine(Request, Vertices[4], Vertices[6], Color);
	AddLine(Request, Vertices[6], Vertices[2], Color);
	AddLine(Request, Vertices[2], Vertices[0], Color);
	AddLine(Request, Vertices[1], Vertices[5], Color);
	AddLine(Request, Vertices[5], Vertices[7], Color);
	AddLine(Request, Vertices[7], Vertices[3], Color);
	AddLine(Request, Vertices[3], Vertices[1], Color);
	AddLine(Request, Vertices[0], Vertices[1], Color);
	AddLine(Request, Vertices[4], Vertices[5], Color);
	AddLine(Request, Vertices[6], Vertices[7], Color);
	AddLine(Request, Vertices[2], Vertices[3], Color);
}

bool FDebugLineRenderFeature::Render(FRenderer& Renderer, const FDebugLineRenderRequest& Request)
{
	if (Request.IsEmpty())
	{
		return true;
	}

	ID3D11Device* Device = Renderer.GetDevice();
	ID3D11DeviceContext* DeviceContext = Renderer.GetDeviceContext();
	FMaterial* DefaultMaterial = Renderer.GetDefaultMaterial();
	if (!Device || !DeviceContext || !DefaultMaterial)
	{
		return false;
	}

	TArray<FVertex> LineVertices;
	LineVertices.reserve(Request.Lines.size() * 2);
	for (const FDebugLineRenderItem& Item : Request.Lines)
	{
		LineVertices.push_back({ Item.Start, Item.Color, FVector::ZeroVector });
		LineVertices.push_back({ Item.End, Item.Color, FVector::ZeroVector });
	}

	const UINT Size = static_cast<UINT>(LineVertices.size() * sizeof(FVertex));
	if (LineVertexBuffer && LineVertexBufferSize < Size)
	{
		LineVertexBuffer->Release();
		LineVertexBuffer = nullptr;
		LineVertexBufferSize = 0;
	}

	if (!LineVertexBuffer)
	{
		D3D11_BUFFER_DESC Desc = {};
		Desc.ByteWidth = Size;
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		if (FAILED(Device->CreateBuffer(&Desc, nullptr, &LineVertexBuffer)))
		{
			return false;
		}

		LineVertexBufferSize = Size;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (FAILED(DeviceContext->Map(LineVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		return false;
	}

	memcpy(Mapped.pData, LineVertices.data(), Size);
	DeviceContext->Unmap(LineVertexBuffer, 0);

	Renderer.ShaderManager.Bind(DeviceContext);
	Renderer.SetConstantBuffers();
	DefaultMaterial->Bind(DeviceContext);

	UINT Stride = sizeof(FVertex);
	UINT Offset = 0;
	DeviceContext->IASetVertexBuffers(0, 1, &LineVertexBuffer, &Stride, &Offset);
	DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer.UpdateObjectConstantBuffer(FMatrix::Identity);
	DeviceContext->Draw(static_cast<UINT>(LineVertices.size()), 0);
	return true;
}

void FDebugLineRenderFeature::Release()
{
	if (LineVertexBuffer)
	{
		LineVertexBuffer->Release();
		LineVertexBuffer = nullptr;
	}

	LineVertexBufferSize = 0;
}
