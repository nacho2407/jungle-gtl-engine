#pragma once

/*
	Vertex Buffer와 Constant Buffer를 관리하는 Class 입니다.
	추후에 Index Buffer, Structured Buffer 등 다양한 Buffer들을 관리하는 Class로 확장할 수 있습니다.
	또한 코드가 길어지면, 각 Buffer들의 source를 분리할 수도 있습니다. (이후)
*/

#include "Render/Common/RenderTypes.h"

#include "Core/CoreMinimal.h"
#include "Render/Resource/VertexTypes.h"


class FVertexBuffer
{
public:
	void Create(ID3D11Device* InDevice, const TArray<FVertex>&, uint32 InByteWidth, uint32 InStride);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth);

	uint32 GetVertexCount() const { return VertexCount; }
	uint32 GetStride() const { return Stride; }

	ID3D11Buffer* GetBuffer() const;

private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 VertexCount = 0;
	uint32 Stride = 0;
};

class FConstantBuffer
{
public:
	void Create(ID3D11Device* InDevice, uint32 InByteWidth);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const void * InData, uint32 InByteWidth);

	ID3D11Buffer* GetBuffer();

private:
	ID3D11Buffer* Buffer = nullptr;
};


class FIndexBuffer
{
public:
	void Create(ID3D11Device* InDevice, const TArray<uint32>& InData, uint32 InByteWidth);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth);

	uint32 GetIndexCount() const { return IndexCount; }
	ID3D11Buffer* GetBuffer() const;

private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 IndexCount = 0;
};

//	하나의 PrimitiveComponent에서 사용할 Mesh Buffer입니다. Vertex Buffer와 Index Buffer를 포함합니다.
//	종류에 따라 달라질 수 있음.
class FMeshBuffer
{
public:
	void Create(ID3D11Device* InDevice, const FMeshData& InMeshData);
	void Release();

	FVertexBuffer& GetVertexBuffer() { return VertexBuffer; }
	FIndexBuffer& GetIndexBuffer() { return IndexBuffer; }
	const FVertexBuffer& GetVertexBuffer() const { return VertexBuffer; }
	const FIndexBuffer& GetIndexBuffer() const { return IndexBuffer; }
	bool IsValid() const { return VertexBuffer.GetBuffer() != nullptr && VertexBuffer.GetVertexCount() > 0; }

private:
	FVertexBuffer VertexBuffer;
	FIndexBuffer IndexBuffer;
};
