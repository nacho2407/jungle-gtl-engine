#include "Buffer.h"
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

	VertexBuffer.Create(InDevice, InMeshData.Vertices, static_cast<uint32>(sizeof(FVertex) * InMeshData.Vertices.size()), sizeof(FVertex));
	if (!InMeshData.Indices.empty())
	{
		IndexBuffer.Create(InDevice, InMeshData.Indices, static_cast<uint32>(sizeof(uint32) * InMeshData.Indices.size()));
	}
}

void FMeshBuffer::CreateForStaticMesh(ID3D11Device* InDevice, const TArray<FNormalVertex>& InVertices, const TArray<uint32>& InIndices)
{
	if (InVertices.empty() || !InDevice)
	{
		return;
	}

	const uint32 Stride     = sizeof(FNormalVertex);
	const uint32 ByteWidth  = static_cast<uint32>(Stride * InVertices.size());

	D3D11_BUFFER_DESC Desc  = {};
	Desc.ByteWidth           = ByteWidth;
	Desc.Usage               = D3D11_USAGE_IMMUTABLE;
	Desc.BindFlags           = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA SRD = { InVertices.data() };

	ID3D11Buffer* RawBuffer = nullptr;
	if (SUCCEEDED(InDevice->CreateBuffer(&Desc, &SRD, &RawBuffer)))
	{
		VertexBuffer.SetRaw(RawBuffer, static_cast<uint32>(InVertices.size()), Stride);
	}

	if (!InIndices.empty())
	{
		IndexBuffer.Create(InDevice, InIndices, static_cast<uint32>(sizeof(uint32) * InIndices.size()));
	}
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
	Stride = InStride;
}

void FVertexBuffer::SetRaw(ID3D11Buffer* InBuffer, uint32 InVertexCount, uint32 InStride)
{
	Release();
	Buffer.Attach(InBuffer);
	VertexCount = InVertexCount;
	Stride      = InStride;
}

void FVertexBuffer::Release()
{
	Buffer.Reset();
}

//	 Vertex buffer�� Immutable�� ���������Ƿ� ������Ʈ�� �Ұ�. ������Ʈ�� �ʿ��ϴٸ� Dynamic���� ����
void FVertexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth)
{
	//	 Do nothing
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

void FStructuredBuffer::Create(ID3D11Device* InDevice, uint32 InElementSize, uint32 InMaxElements) {
	if (InElementSize == 0) {
		Release();
		return;
	}

	ElementSize = InElementSize;

	D3D11_BUFFER_DESC desc = {};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = InElementSize * InMaxElements;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    desc.StructureByteStride = InElementSize;   

	InDevice->CreateBuffer(&desc, nullptr, Buffer.ReleaseAndGetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = InMaxElements;
	InDevice->CreateShaderResourceView(Buffer.Get(), &srvDesc, SRV.ReleaseAndGetAddressOf());
}

void FStructuredBuffer::Release() {
	Buffer.Reset();
	SRV.Reset();
	Count = 0;
	ElementSize = 0;
}

void FStructuredBuffer::Update(ID3D11DeviceContext* InContext, const void* InData, uint32 InElementCount) {
	if (Buffer) {
		D3D11_MAPPED_SUBRESOURCE structuredMSR;
        InContext->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &structuredMSR);
        std::memcpy(structuredMSR.pData, InData, InElementCount * ElementSize);
		Count = InElementCount;
        InContext->Unmap(Buffer.Get(), 0);
	}
}

ID3D11ShaderResourceView* FStructuredBuffer::GetSRV() const {
	return SRV.Get();
}

ID3D11Buffer * FIndexBuffer::GetBuffer() const
{
	return Buffer.Get();
}

#pragma endregion

