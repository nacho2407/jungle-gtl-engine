#pragma once

/*
	Vertex Buffer와 Constant Buffer를 관리하는 Class 입니다.
	추후에 Index Buffer, Structured Buffer 등 다양한 Buffer들을 관리하는 Class로 확장할 수 있습니다.
	또한 코드가 길어지면, 각 Buffer들의 source를 분리할 수도 있습니다. (이후)
*/

#include "Render/Common/ComPtr.h"

#include "Core/CoreMinimal.h"
#include "Core/CoreTypes.h"
#include "Render/Resource/VertexTypes.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11ShaderResourceView;
struct ID3D11UnorderedAccessView;

class FVertexBuffer
{
public:
	void Create(ID3D11Device* InDevice, const TArray<FVertex>&, uint32 InByteWidth, uint32 InStride);

	// Vertex 타입을 고정하지 않고 raw pointer + stride로 생성합니다.
	// StaticMesh(FNormalVertex) / SkeletalMesh(FSkeletalMeshVertex)를 같은 버퍼 클래스로 처리하기 위한 경로입니다.
	void CreateRaw(ID3D11Device* InDevice, const void* InData, uint32 InVertexCount, uint32 InStride, bool bDynamic);
	void SetRaw(ID3D11Buffer* InBuffer, uint32 InVertexCount, uint32 InStride);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const TArray<uint32>& InData, uint32 InByteWidth);

	// CPU Skinning처럼 매 프레임 Vertex Data가 바뀌는 경우 Dynamic VB를 갱신합니다.
	void UpdateRaw(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InVertexCount);

	uint32 GetVertexCount() const { return VertexCount; }
	uint32 GetVertexCapacity() const { return VertexCapacity; }
	uint32 GetStride() const { return Stride; }

	ID3D11Buffer* GetBuffer() const;

private:
	TComPtr<ID3D11Buffer> Buffer;
	uint32 VertexCount = 0;
	uint32 VertexCapacity = 0;
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
	TComPtr<ID3D11Buffer> Buffer;
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
	TComPtr<ID3D11Buffer> Buffer;
	uint32 IndexCount = 0;
};

//	하나의 PrimitiveComponent에서 사용할 Mesh Buffer입니다. Vertex Buffer와 Index Buffer를 포함합니다.
//	종류에 따라 달라질 수 있음.
class FMeshBuffer
{
public:
	void Create(ID3D11Device* InDevice, const FMeshData& InMeshData);

	// GPU에 한 번 올리고 거의 바뀌지 않는 메시용입니다. StaticMesh / GPU Skinning 원본 버퍼에 사용합니다.
	void CreateImmutableVertexBuffer(ID3D11Device* InDevice, const void* InVertexData, uint32 InVertexCount, uint32 InVertexStride, const TArray<uint32>& InIndices);

	// CPU Skinning / Procedural처럼 Vertex Data를 계속 갱신해야 하는 메시용입니다.
	void CreateDynamicVertexBuffer(ID3D11Device* InDevice, uint32 InMaxVertexCount, uint32 InVertexStride, const TArray<uint32>& InIndices);
	void UpdateDynamicVertexBuffer(ID3D11DeviceContext* InDeviceContext, const void* InVertexData, uint32 InVertexCount);
	void Release();

	template <typename TVertex>
	void CreateImmutableVertices(ID3D11Device* InDevice, const TArray<TVertex>& InVertices, const TArray<uint32>& InIndices)
	{
		CreateImmutableVertexBuffer(InDevice, InVertices.data(), static_cast<uint32>(InVertices.size()), sizeof(TVertex), InIndices);
	}

	template <typename TVertex>
	void CreateDynamicVertices(ID3D11Device* InDevice, uint32 InMaxVertexCount, const TArray<uint32>& InIndices)
	{
		CreateDynamicVertexBuffer(InDevice, InMaxVertexCount, sizeof(TVertex), InIndices);
	}

	template <typename TVertex>
	void UpdateDynamicVertices(ID3D11DeviceContext* InDeviceContext, const TArray<TVertex>& InVertices)
	{
		UpdateDynamicVertexBuffer(InDeviceContext, InVertices.data(), static_cast<uint32>(InVertices.size()));
	}

	FVertexBuffer& GetVertexBuffer() { return VertexBuffer; }
	FIndexBuffer& GetIndexBuffer() { return IndexBuffer; }
	const FVertexBuffer& GetVertexBuffer() const { return VertexBuffer; }
	const FIndexBuffer& GetIndexBuffer() const { return IndexBuffer; }
	bool IsValid() const { return VertexBuffer.GetBuffer() != nullptr && VertexBuffer.GetVertexCount() > 0; }

private:
	FVertexBuffer VertexBuffer;
	FIndexBuffer IndexBuffer;
};

class FStructuredBuffer
{
public:
    void Create(ID3D11Device* InDevice, uint32 InElementSize, uint32 InMaxElements, bool bEnableUAV = false);
	void Update(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InElementCount);
	void Release();

	ID3D11ShaderResourceView* GetSRV() const;
    ID3D11UnorderedAccessView* GetUAV() const;
    uint32 GetCount() const { return Count; }

private:
	TComPtr<ID3D11Buffer>			  Buffer;
	TComPtr<ID3D11ShaderResourceView> SRV;
    TComPtr<ID3D11UnorderedAccessView> UAV;
	uint32							  Count = 0;
	uint32							  ElementSize = 0;
};
