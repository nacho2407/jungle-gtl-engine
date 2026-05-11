#include "Buffer.h"
#include <d3d11.h>
#include <cstring>
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

void FVertexBuffer::Create(ID3D11Device* InDevice, const TArray<FVertex> & InData, uint32 InByteWidth, uint32 InStride)
{
	if (InData.empty() || InByteWidth == 0)
	{
		Release();
		VertexCount = 0;
		Stride = InStride;
		return;
	}

	D3D11_BUFFER_DESC vertexBufferDesc = {};
	vertexBufferDesc.ByteWidth = InByteWidth;
	vertexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vertexBufferSRD = { InData.data() };
	
	HRESULT hr = InDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferSRD, Buffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
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
	Stride      = InStride;
}

void FVertexBuffer::Release()
{
	Buffer.Reset();
	VertexCount = 0;
	VertexCapacity = 0;
	Stride = 0;
}

//	 Vertex buffer�� Immutable�� ���������Ƿ� ������Ʈ�� �Ұ�. ������Ʈ�� �ʿ��ϴٸ� Dynamic���� ����
void FVertexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth)
{
	//	 Do nothing
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

//	 Constant buffer�� Dynamic���� �����Ͽ� ������Ʈ�� �����ϵ��� ����
void FConstantBuffer::Create(ID3D11Device* InDevice, uint32 InByteWidth)
{
	D3D11_BUFFER_DESC constantBufferDesc = {};

	constantBufferDesc.ByteWidth = (InByteWidth + 0xf) & 0xfffffff0;
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;	
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	InDevice->CreateBuffer(&constantBufferDesc, nullptr, Buffer.ReleaseAndGetAddressOf());
}

void FConstantBuffer::Release()
{
	Buffer.Reset();
}

//	Constant buffer�� Dynamic���� ���������Ƿ� ������Ʈ�� ����. ������Ʈ�� �ʿ��ϴٸ� Map/Unmap�� �̿��Ͽ� ������Ʈ
//	InData�� Constant buffer�� ������Ʈ�� �������� �������Դϴ�. InByteWidth�� ������Ʈ�� �������� �� byte ũ���Դϴ�.
//	��, InData�� FPerObjectConstants ����ü�� �������Դϴ�.
void FConstantBuffer::Update(ID3D11DeviceContext* InDeviceContext, const void * InData, uint32 InByteWidth)
{
	if (Buffer)
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
		InDeviceContext->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);

		std::memcpy(constantbufferMSR.pData, InData, InByteWidth);

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

	D3D11_BUFFER_DESC indexBufferDesc = {};

	indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	indexBufferDesc.ByteWidth = InByteWidth;	//	NOTE : Total byte width of the buffer, not the count of indices
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA indexBufferSRD = { InData.data() };

	HRESULT hr = InDevice->CreateBuffer(&indexBufferDesc, &indexBufferSRD, Buffer.ReleaseAndGetAddressOf());
	if (FAILED(hr))
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
	//	 Do nothing
}

void FStructuredBuffer::Create(ID3D11Device* InDevice, uint32 InElementSize, uint32 InMaxElements, bool bEnableUAV) {
	if (InElementSize == 0) {
		Release();
		return;
	}

	ElementSize = InElementSize;

	D3D11_BUFFER_DESC desc = {};
	desc.ByteWidth = InElementSize * InMaxElements;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = InElementSize;

	if (bEnableUAV) {
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
		desc.CPUAccessFlags = 0;
	}
	else {
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}

	InDevice->CreateBuffer(&desc, nullptr, Buffer.ReleaseAndGetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = InMaxElements;
	InDevice->CreateShaderResourceView(Buffer.Get(), &srvDesc, SRV.ReleaseAndGetAddressOf());

	if (bEnableUAV)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = InMaxElements;
		InDevice->CreateUnorderedAccessView(Buffer.Get(), &uavDesc, UAV.ReleaseAndGetAddressOf());
	}
}

void FStructuredBuffer::Release() {
	Buffer.Reset();
	UAV.Reset();
	SRV.Reset();
	Count = 0;
	ElementSize = 0;
}

void FStructuredBuffer::Update(ID3D11DeviceContext* InContext, const void* InData, uint32 InElementCount)
{
    if (!Buffer || !InData)
        return;

    D3D11_BUFFER_DESC desc;
    Buffer->GetDesc(&desc);

    if (desc.Usage == D3D11_USAGE_DYNAMIC)
    {
        D3D11_MAPPED_SUBRESOURCE msr;
        if (SUCCEEDED(InContext->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
        {
            std::memcpy(msr.pData, InData, InElementCount * ElementSize);
            InContext->Unmap(Buffer.Get(), 0);
        }
    }
    else
    {
        InContext->UpdateSubresource(Buffer.Get(), 0, nullptr, InData, 0, 0);
    }
    Count = InElementCount;
}

ID3D11ShaderResourceView* FStructuredBuffer::GetSRV() const {
	return SRV.Get();
}

ID3D11UnorderedAccessView* FStructuredBuffer::GetUAV() const
{
    return UAV.Get();
}

ID3D11Buffer * FIndexBuffer::GetBuffer() const
{
	return Buffer.Get();
}

#pragma endregion

