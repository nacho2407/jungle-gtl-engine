#pragma once

#include "Render/Types/RenderTypes.h"
#include "Core/CoreTypes.h"
#include "Render/Types/VertexTypes.h"

class FVertexBuffer
{
public:
	FVertexBuffer() = default;
	~FVertexBuffer() { Release(); }

	FVertexBuffer(const FVertexBuffer&) = delete;
	FVertexBuffer& operator=(const FVertexBuffer&) = delete;
	FVertexBuffer(FVertexBuffer&&) noexcept;
	FVertexBuffer& operator=(FVertexBuffer&&) noexcept;

	void Create(ID3D11Device* InDevice, const void* InData, uint32 InVertexCount, uint32 InByteWidth, uint32 InStride);
	void Release();

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
	FConstantBuffer() = default;
	~FConstantBuffer() { Release(); }

	FConstantBuffer(const FConstantBuffer&) = delete;
	FConstantBuffer& operator=(const FConstantBuffer&) = delete;
	FConstantBuffer(FConstantBuffer&&) noexcept;
	FConstantBuffer& operator=(FConstantBuffer&&) noexcept;

	void Create(ID3D11Device* InDevice, uint32 InByteWidth);
	void Release();

	void Update(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InByteWidth);

	ID3D11Buffer* GetBuffer();

private:
	ID3D11Buffer* Buffer = nullptr;
};


class FIndexBuffer
{
public:
	FIndexBuffer() = default;
	~FIndexBuffer() { Release(); }

	FIndexBuffer(const FIndexBuffer&) = delete;
	FIndexBuffer& operator=(const FIndexBuffer&) = delete;
	FIndexBuffer(FIndexBuffer&&) noexcept;
	FIndexBuffer& operator=(FIndexBuffer&&) noexcept;

	void Create(ID3D11Device* InDevice, const void* InData, uint32 InIndexCount, uint32 InByteWidth);
	void Release();

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
	FMeshBuffer() = default;
	~FMeshBuffer() { Release(); }

	FMeshBuffer(const FMeshBuffer&) = delete;
	FMeshBuffer& operator=(const FMeshBuffer&) = delete;
	FMeshBuffer(FMeshBuffer&&) = default;
	FMeshBuffer& operator=(FMeshBuffer&&) = default;

	template<typename VertexType>
	void Create(ID3D11Device* InDevice, const TMeshData<VertexType>& InMeshData);
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

// ============================================================
// Dynamic Buffers — 매 프레임 CPU 데이터를 GPU에 업로드하는 버퍼 (Batcher용)
// ============================================================

class FDynamicVertexBuffer
{
public:
	FDynamicVertexBuffer() = default;
	~FDynamicVertexBuffer() { Release(); }

	FDynamicVertexBuffer(const FDynamicVertexBuffer&) = delete;
	FDynamicVertexBuffer& operator=(const FDynamicVertexBuffer&) = delete;

	void Create(ID3D11Device* InDevice, uint32 InMaxCount, uint32 InStride);
	void Release();

	// RequiredCount > MaxCount 이면 2배 확장 후 재생성
	void EnsureCapacity(ID3D11Device* InDevice, uint32 RequiredCount);
	// Map → memcpy → Unmap (WRITE_DISCARD)
	bool Update(ID3D11DeviceContext* Context, const void* Data, uint32 Count);
	// IASetVertexBuffers
	void Bind(ID3D11DeviceContext* Context, uint32 Slot = 0);

	uint32 GetMaxCount() const { return MaxCount; }

private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 MaxCount = 0;
	uint32 Stride = 0;
};

class FDynamicIndexBuffer
{
public:
	FDynamicIndexBuffer() = default;
	~FDynamicIndexBuffer() { Release(); }

	FDynamicIndexBuffer(const FDynamicIndexBuffer&) = delete;
	FDynamicIndexBuffer& operator=(const FDynamicIndexBuffer&) = delete;

	void Create(ID3D11Device* InDevice, uint32 InMaxCount);
	void Release();

	void EnsureCapacity(ID3D11Device* InDevice, uint32 RequiredCount);
	bool Update(ID3D11DeviceContext* Context, const void* Data, uint32 Count);
	// IASetIndexBuffer (DXGI_FORMAT_R32_UINT)
	void Bind(ID3D11DeviceContext* Context);

	uint32 GetMaxCount() const { return MaxCount; }

private:
	ID3D11Buffer* Buffer = nullptr;
	uint32 MaxCount = 0;
};

// ============================================================

template<typename VertexType>
void FMeshBuffer::Create(ID3D11Device* InDevice, const TMeshData<VertexType>& InMeshData)
{
	Release();

	if (InMeshData.Vertices.empty())
	{
		return;
	}

	uint32 VertexCount = static_cast<uint32>(InMeshData.Vertices.size());
	uint32 VertexByteWidth = VertexCount * sizeof(VertexType);

	VertexBuffer.Create(InDevice, InMeshData.Vertices.data(), VertexCount, VertexByteWidth, sizeof(VertexType));

	if (!InMeshData.Indices.empty())
	{
		uint32 IndexCount = static_cast<uint32>(InMeshData.Indices.size());
		uint32 IndexByteWidth = IndexCount * sizeof(uint32);

		IndexBuffer.Create(InDevice, InMeshData.Indices.data(), IndexCount, IndexByteWidth);
	}
}
