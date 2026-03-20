#include "Buffer.h"

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
	
	HRESULT hr = InDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferSRD, &Buffer);
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

void FVertexBuffer::Release()
{
	if (Buffer)
	{
		Buffer->Release();
		Buffer = nullptr;
	}
}

//	 Vertex buffer는 Immutable로 생성했으므로 업데이트가 불가. 업데이트가 필요하다면 Dynamic으로 생성
void FVertexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth)
{
	//	 Do nothing
}

ID3D11Buffer* FVertexBuffer::GetBuffer() const
{
	return Buffer;
}

#pragma endregion

#pragma region __FCONSTANTBUFFER__

//	 Constant buffer는 Dynamic으로 생성하여 업데이트가 가능하도록 구현
void FConstantBuffer::Create(ID3D11Device* InDevice, uint32 InByteWidth)
{
	D3D11_BUFFER_DESC constantBufferDesc = {};

	constantBufferDesc.ByteWidth = (InByteWidth + 0xf) & 0xfffffff0;
	constantBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	constantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;	
	constantBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	InDevice->CreateBuffer(&constantBufferDesc, nullptr, &Buffer);
}

void FConstantBuffer::Release()
{
	if(Buffer)
	{
		Buffer->Release();
		Buffer = nullptr;
	}
}

//	Constant buffer는 Dynamic으로 생성했으므로 업데이트가 가능. 업데이트가 필요하다면 Map/Unmap을 이용하여 업데이트
//	InData는 Constant buffer에 업데이트할 데이터의 포인터입니다. InByteWidth는 업데이트할 데이터의 총 byte 크기입니다.
//	즉, InData는 FPerObjectConstants 구조체의 포인터입니다.
void FConstantBuffer::Update(ID3D11DeviceContext* InDeviceContext, const void * InData, uint32 InByteWidth)
{
	if (Buffer)
	{
		D3D11_MAPPED_SUBRESOURCE constantbufferMSR;
		InDeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &constantbufferMSR);

		std::memcpy(constantbufferMSR.pData, InData, InByteWidth);

		InDeviceContext->Unmap(Buffer, 0);
	}
}

ID3D11Buffer* FConstantBuffer::GetBuffer() 
{
	return Buffer;
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

	HRESULT hr = InDevice->CreateBuffer(&indexBufferDesc, &indexBufferSRD, &Buffer);
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
	if (Buffer)
	{
		Buffer->Release();
		Buffer = nullptr;
	}
}

void FIndexBuffer::Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth)
{
	//	 Do nothing
}

ID3D11Buffer * FIndexBuffer::GetBuffer() const
{
	return Buffer;
}

#pragma endregion