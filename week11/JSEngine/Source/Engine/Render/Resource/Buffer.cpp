#include "Buffer.h"

#include <cstring>
#include <d3d11.h>

#pragma region __FMESHBUFFER__

void FMeshBuffer::Create(ID3D11Device* InDevice, const FMeshData& InMeshData)
{
	if (InMeshData.Vertices.empty())
	{
		VertexBuffer.Release();
		IndexBuffer.Release();
		return;
	}

	CreateImmutableVertices(InDevice, InMeshData.Vertices, InMeshData.Indices);
}

void FMeshBuffer::CreateImmutableVertexBuffer(ID3D11Device* InDevice, const void* InVertexData, uint32 InVertexCount, uint32 InVertexStride, const TArray<uint32>& InIndices)
{
	if (!InDevice || !InVertexData || InVertexCount == 0 || InVertexStride == 0)
	{
		Release();
		return;
	}

	VertexBuffer.CreateRaw(InDevice, InVertexData, InVertexCount, InVertexStride, false);

	if (!InIndices.empty())
	{
		IndexBuffer.Create(InDevice, InIndices, static_cast<uint32>(sizeof(uint32) * InIndices.size()));
	}
}

void FMeshBuffer::CreateDynamicVertexBuffer(ID3D11Device* InDevice, uint32 InMaxVertexCount, uint32 InVertexStride, const TArray<uint32>& InIndices)
{
	if (!InDevice || InMaxVertexCount == 0 || InVertexStride == 0)
	{
		Release();
		return;
	}

	VertexBuffer.CreateRaw(InDevice, nullptr, InMaxVertexCount, InVertexStride, true);

	if (!InIndices.empty())
	{
		IndexBuffer.Create(InDevice, InIndices, static_cast<uint32>(sizeof(uint32) * InIndices.size()));
	}
}

void FMeshBuffer::UpdateDynamicVertexBuffer(ID3D11DeviceContext* InDeviceContext, const void* InVertexData, uint32 InVertexCount)
{
	VertexBuffer.UpdateRaw(InDeviceContext, InVertexData, InVertexCount);
}

void FMeshBuffer::Release()
{
	VertexBuffer.Release();
	IndexBuffer.Release();
}

#pragma endregion

#pragma region __FVERTEXBUFFER__

void FVertexBuffer::Create(ID3D11Device* InDevice, const TArray<FVertex>& InData, uint32 InByteWidth, uint32 InStride)
{
	if (InData.empty() || InByteWidth == 0)
	{
		Release();
		VertexCount = 0;
		Stride = InStride;
		return;
	}

	D3D11_BUFFER_DESC VertexBufferDesc = {};
	VertexBufferDesc.ByteWidth = InByteWidth;
	VertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VertexBufferSRD = { InData.data() };

	HRESULT Hr = InDevice->CreateBuffer(&VertexBufferDesc, &VertexBufferSRD, Buffer.ReleaseAndGetAddressOf());
	if (FAILED(Hr))
	{
		Release();
		VertexCount = 0;
		Stride = InStride;
		return;
	}

	VertexCount = static_cast<uint32>(InData.size());
	VertexCapacity = VertexCount;
	Stride = InStride;
}

void FVertexBuffer::CreateRaw(ID3D11Device* InDevice, const void* InData, uint32 InVertexCount, uint32 InStride, bool bDynamic)
{
	if (!InDevice || InVertexCount == 0 || InStride == 0 || (!bDynamic && !InData))
	{
		Release();
		VertexCount = 0;
		VertexCapacity = 0;
		Stride = InStride;
		return;
	}

	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = InVertexCount * InStride;
	Desc.Usage = bDynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Desc.CPUAccessFlags = bDynamic ? D3D11_CPU_ACCESS_WRITE : 0;

	D3D11_SUBRESOURCE_DATA SRD = {};
	SRD.pSysMem = InData;

	HRESULT Hr = InDevice->CreateBuffer(&Desc, InData ? &SRD : nullptr, Buffer.ReleaseAndGetAddressOf());
	if (FAILED(Hr))
	{
		Release();
		VertexCount = 0;
		VertexCapacity = 0;
		Stride = InStride;
		return;
	}

	VertexCount = bDynamic ? 0 : InVertexCount;
	VertexCapacity = InVertexCount;
	Stride = InStride;
}

void FVertexBuffer::SetRaw(ID3D11Buffer* InBuffer, uint32 InVertexCount, uint32 InStride)
{
	Release();
	Buffer.Attach(InBuffer);
	VertexCount = InVertexCount;
	VertexCapacity = InVertexCount;
	Stride = InStride;
}

void FVertexBuffer::Release()
{
	Buffer.Reset();
	VertexCount = 0;
	VertexCapacity = 0;
	Stride = 0;
}

void FVertexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth)
{
	(void)InDeviceContext;
	(void)InData;
	(void)InByteWidth;
}

void FVertexBuffer::UpdateRaw(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InVertexCount)
{
	if (!InDeviceContext || Buffer.Get() == nullptr || !InData || InVertexCount == 0 || InVertexCount > VertexCapacity || Stride == 0)
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (SUCCEEDED(InDeviceContext->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		std::memcpy(Mapped.pData, InData, static_cast<size_t>(InVertexCount) * Stride);
		InDeviceContext->Unmap(Buffer.Get(), 0);
		VertexCount = InVertexCount;
	}
}

ID3D11Buffer* FVertexBuffer::GetBuffer() const
{
	return Buffer.Get();
}

#pragma endregion

#pragma region __FCONSTANTBUFFER__

void FConstantBuffer::Create(ID3D11Device* InDevice, uint32 InByteWidth)
{
	D3D11_BUFFER_DESC ConstantBufferDesc = {};
	ConstantBufferDesc.ByteWidth = (InByteWidth + 0xf) & 0xfffffff0;
	ConstantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	ConstantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	InDevice->CreateBuffer(&ConstantBufferDesc, nullptr, Buffer.ReleaseAndGetAddressOf());
}

void FConstantBuffer::Release()
{
	Buffer.Reset();
}

void FConstantBuffer::Update(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InByteWidth)
{
	if (Buffer)
	{
		D3D11_MAPPED_SUBRESOURCE ConstantBufferMSR;
		InDeviceContext->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ConstantBufferMSR);

		std::memcpy(ConstantBufferMSR.pData, InData, InByteWidth);

		InDeviceContext->Unmap(Buffer.Get(), 0);
	}
}

ID3D11Buffer* FConstantBuffer::GetBuffer()
{
	return Buffer.Get();
}

#pragma endregion

#pragma region __FINDEXBUFFER__

void FIndexBuffer::Create(ID3D11Device* InDevice, const TArray<uint32>& InData, uint32 InByteWidth)
{
	if (InData.empty() || InByteWidth == 0)
	{
		Release();
		IndexCount = 0;
		return;
	}

	D3D11_BUFFER_DESC IndexBufferDesc = {};
	IndexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	IndexBufferDesc.ByteWidth = InByteWidth;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IndexBufferSRD = { InData.data() };

	HRESULT Hr = InDevice->CreateBuffer(&IndexBufferDesc, &IndexBufferSRD, Buffer.ReleaseAndGetAddressOf());
	if (FAILED(Hr))
	{
		Release();
		IndexCount = 0;
		return;
	}

	IndexCount = static_cast<uint32>(InData.size());
}

void FIndexBuffer::Release()
{
	Buffer.Reset();
}

void FIndexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth)
{
	(void)InDeviceContext;
	(void)InData;
	(void)InByteWidth;
}

ID3D11Buffer* FIndexBuffer::GetBuffer() const
{
	return Buffer.Get();
}

#pragma endregion

#pragma region __FSTRUCTUREDBUFFER__

void FStructuredBuffer::Create(ID3D11Device* InDevice, uint32 InElementSize, uint32 InMaxElements, bool bEnableUAV)
{
	if (InElementSize == 0)
	{
		Release();
		return;
	}

	ElementSize = InElementSize;

	D3D11_BUFFER_DESC Desc = {};
	Desc.ByteWidth = InElementSize * InMaxElements;
	Desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	Desc.StructureByteStride = InElementSize;

	if (bEnableUAV)
	{
		Desc.Usage = D3D11_USAGE_DEFAULT;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		Desc.CPUAccessFlags = 0;
	}
	else
	{
		Desc.Usage = D3D11_USAGE_DYNAMIC;
		Desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	InDevice->CreateBuffer(&Desc, nullptr, Buffer.ReleaseAndGetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC SrvDesc = {};
	SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	SrvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SrvDesc.Buffer.FirstElement = 0;
	SrvDesc.Buffer.NumElements = InMaxElements;
	InDevice->CreateShaderResourceView(Buffer.Get(), &SrvDesc, SRV.ReleaseAndGetAddressOf());

	if (bEnableUAV)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC UavDesc = {};
		UavDesc.Format = DXGI_FORMAT_UNKNOWN;
		UavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		UavDesc.Buffer.FirstElement = 0;
		UavDesc.Buffer.NumElements = InMaxElements;
		InDevice->CreateUnorderedAccessView(Buffer.Get(), &UavDesc, UAV.ReleaseAndGetAddressOf());
	}
}

void FStructuredBuffer::Release()
{
	Buffer.Reset();
	UAV.Reset();
	SRV.Reset();
	Count = 0;
	ElementSize = 0;
}

void FStructuredBuffer::Update(ID3D11DeviceContext* InContext, const void* InData, uint32 InElementCount)
{
	if (!Buffer || !InData)
	{
		return;
	}

	D3D11_BUFFER_DESC Desc;
	Buffer->GetDesc(&Desc);

	if (Desc.Usage == D3D11_USAGE_DYNAMIC)
	{
		D3D11_MAPPED_SUBRESOURCE Msr;
		if (SUCCEEDED(InContext->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &Msr)))
		{
			std::memcpy(Msr.pData, InData, InElementCount * ElementSize);
			InContext->Unmap(Buffer.Get(), 0);
		}
	}
	else
	{
		InContext->UpdateSubresource(Buffer.Get(), 0, nullptr, InData, 0, 0);
	}
	Count = InElementCount;
}

ID3D11ShaderResourceView* FStructuredBuffer::GetSRV() const
{
	return SRV.Get();
}

ID3D11UnorderedAccessView* FStructuredBuffer::GetUAV() const
{
	return UAV.Get();
}

#pragma endregion
