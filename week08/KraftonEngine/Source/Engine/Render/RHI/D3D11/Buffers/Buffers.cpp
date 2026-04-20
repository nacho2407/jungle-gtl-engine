#include "Render/RHI/D3D11/Buffers/Buffers.h"

#include "Engine/Profiling/MemoryStats.h"

#include <Windows.h>
#include <cstdio>
#include <cstring>

namespace
{
void BufferDebugLog(const char* Format, ...)
{
    char Buffer[512] = {};
    va_list Args;
    va_start(Args, Format);
    vsnprintf(Buffer, sizeof(Buffer), Format, Args);
    va_end(Args);
    OutputDebugStringA(Buffer);
    OutputDebugStringA("\n");
}
} // namespace

void FMeshBuffer::Release()
{
    VertexBuffer.Release();
    IndexBuffer.Release();
}

FVertexBuffer::FVertexBuffer(FVertexBuffer&& Other) noexcept
    : Buffer(Other.Buffer), VertexCount(Other.VertexCount), Stride(Other.Stride)
{
    Other.Buffer = nullptr;
    Other.VertexCount = 0;
    Other.Stride = 0;
}

FVertexBuffer& FVertexBuffer::operator=(FVertexBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Buffer = Other.Buffer;
        VertexCount = Other.VertexCount;
        Stride = Other.Stride;
        Other.Buffer = nullptr;
        Other.VertexCount = 0;
        Other.Stride = 0;
    }
    return *this;
}

void FVertexBuffer::Create(ID3D11Device* InDevice, const void* InData, uint32 InVertexCount, uint32 InByteWidth, uint32 InStride)
{
    Release();
    if (!InData || InByteWidth == 0)
    {
        VertexCount = 0;
        Stride = InStride;
        return;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = InByteWidth;
    Desc.Usage = D3D11_USAGE_IMMUTABLE;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA Data = { InData };
    HRESULT hr = InDevice->CreateBuffer(&Desc, &Data, &Buffer);
    if (FAILED(hr))
    {
        BufferDebugLog("[VertexBuffer] CreateBuffer failed hr=0x%08X vertexCount=%u byteWidth=%u stride=%u device=%p data=%p",
                       static_cast<unsigned>(hr), InVertexCount, InByteWidth, InStride, InDevice, InData);
        VertexCount = 0;
        Stride = InStride;
        return;
    }

    VertexCount = InVertexCount;
    Stride = InStride;
    MemoryStats::AddVertexBufferMemory(InByteWidth);
}

void FVertexBuffer::Release()
{
    if (Buffer)
    {
        D3D11_BUFFER_DESC Desc = {};
        Buffer->GetDesc(&Desc);
        MemoryStats::SubVertexBufferMemory(Desc.ByteWidth);
        Buffer->Release();
        Buffer = nullptr;
    }
    VertexCount = 0;
}

ID3D11Buffer* FVertexBuffer::GetBuffer() const
{
    return Buffer;
}

FConstantBuffer::FConstantBuffer(FConstantBuffer&& Other) noexcept
    : Buffer(Other.Buffer)
{
    Other.Buffer = nullptr;
}

FConstantBuffer& FConstantBuffer::operator=(FConstantBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Buffer = Other.Buffer;
        Other.Buffer = nullptr;
    }
    return *this;
}

void FConstantBuffer::Create(ID3D11Device* InDevice, uint32 InByteWidth)
{
    Release();
    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = (InByteWidth + 0xf) & 0xfffffff0;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    InDevice->CreateBuffer(&Desc, nullptr, &Buffer);
}

void FConstantBuffer::Release()
{
    if (Buffer)
    {
        Buffer->Release();
        Buffer = nullptr;
    }
}

void FConstantBuffer::Update(ID3D11DeviceContext* InDeviceContext, const void* InData, uint32 InByteWidth)
{
    if (!Buffer)
    {
        return;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    HRESULT hr = InDeviceContext->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
    if (FAILED(hr))
    {
        return;
    }

    std::memcpy(Mapped.pData, InData, InByteWidth);
    InDeviceContext->Unmap(Buffer, 0);
}

ID3D11Buffer* FConstantBuffer::GetBuffer()
{
    return Buffer;
}

FIndexBuffer::FIndexBuffer(FIndexBuffer&& Other) noexcept
    : Buffer(Other.Buffer), IndexCount(Other.IndexCount)
{
    Other.Buffer = nullptr;
    Other.IndexCount = 0;
}

FIndexBuffer& FIndexBuffer::operator=(FIndexBuffer&& Other) noexcept
{
    if (this != &Other)
    {
        Release();
        Buffer = Other.Buffer;
        IndexCount = Other.IndexCount;
        Other.Buffer = nullptr;
        Other.IndexCount = 0;
    }
    return *this;
}

void FIndexBuffer::Create(ID3D11Device* InDevice, const void* InData, uint32 InIndexCount, uint32 InByteWidth)
{
    Release();
    if (!InData || InByteWidth == 0 || InIndexCount == 0)
    {
        IndexCount = 0;
        return;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.Usage = D3D11_USAGE_IMMUTABLE;
    Desc.ByteWidth = InByteWidth;
    Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA Data = { InData };
    HRESULT hr = InDevice->CreateBuffer(&Desc, &Data, &Buffer);
    if (FAILED(hr))
    {
        BufferDebugLog("[IndexBuffer] CreateBuffer failed hr=0x%08X indexCount=%u byteWidth=%u device=%p data=%p",
                       static_cast<unsigned>(hr), InIndexCount, InByteWidth, InDevice, InData);
        IndexCount = 0;
        return;
    }

    IndexCount = InIndexCount;
    MemoryStats::AddIndexBufferMemory(InByteWidth);
}

void FIndexBuffer::Release()
{
    if (Buffer)
    {
        D3D11_BUFFER_DESC Desc = {};
        Buffer->GetDesc(&Desc);
        MemoryStats::SubIndexBufferMemory(Desc.ByteWidth);
        Buffer->Release();
        Buffer = nullptr;
    }
    IndexCount = 0;
}

ID3D11Buffer* FIndexBuffer::GetBuffer() const
{
    return Buffer;
}

void FDynamicVertexBuffer::Create(ID3D11Device* InDevice, uint32 InMaxCount, uint32 InStride)
{
    Release();
    Stride = InStride;
    MaxCount = InMaxCount;
    if (!InDevice || MaxCount == 0 || Stride == 0)
    {
        return;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = Stride * MaxCount;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    InDevice->CreateBuffer(&Desc, nullptr, &Buffer);
}

void FDynamicVertexBuffer::Release()
{
    if (Buffer)
    {
        Buffer->Release();
        Buffer = nullptr;
    }
    MaxCount = 0;
}

void FDynamicVertexBuffer::EnsureCapacity(ID3D11Device* InDevice, uint32 RequiredCount)
{
    if (Buffer && RequiredCount <= MaxCount)
    {
        return;
    }
    uint32 NewMax = MaxCount > 0 ? MaxCount : 256;
    while (NewMax < RequiredCount)
    {
        NewMax *= 2;
    }
    Create(InDevice, NewMax, Stride);
}

bool FDynamicVertexBuffer::Update(ID3D11DeviceContext* Context, const void* Data, uint32 Count)
{
    if (!Buffer || !Context || !Data || Count == 0)
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (FAILED(Context->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        return false;
    }
    memcpy(Mapped.pData, Data, static_cast<size_t>(Stride) * Count);
    Context->Unmap(Buffer, 0);
    return true;
}

void FDynamicVertexBuffer::Bind(ID3D11DeviceContext* Context, uint32 Slot)
{
    UINT Offset = 0;
    Context->IASetVertexBuffers(Slot, 1, &Buffer, &Stride, &Offset);
}

void FDynamicIndexBuffer::Create(ID3D11Device* InDevice, uint32 InMaxCount)
{
    Release();
    MaxCount = InMaxCount;
    if (!InDevice || MaxCount == 0)
    {
        return;
    }

    D3D11_BUFFER_DESC Desc = {};
    Desc.ByteWidth = sizeof(uint32) * MaxCount;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    InDevice->CreateBuffer(&Desc, nullptr, &Buffer);
}

void FDynamicIndexBuffer::Release()
{
    if (Buffer)
    {
        Buffer->Release();
        Buffer = nullptr;
    }
    MaxCount = 0;
}

void FDynamicIndexBuffer::EnsureCapacity(ID3D11Device* InDevice, uint32 RequiredCount)
{
    if (Buffer && RequiredCount <= MaxCount)
    {
        return;
    }
    uint32 NewMax = MaxCount > 0 ? MaxCount : 256;
    while (NewMax < RequiredCount)
    {
        NewMax *= 2;
    }
    Create(InDevice, NewMax);
}

bool FDynamicIndexBuffer::Update(ID3D11DeviceContext* Context, const void* Data, uint32 Count)
{
    if (!Buffer || !Context || !Data || Count == 0)
    {
        return false;
    }

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    if (FAILED(Context->Map(Buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
    {
        return false;
    }
    memcpy(Mapped.pData, Data, sizeof(uint32) * Count);
    Context->Unmap(Buffer, 0);
    return true;
}

void FDynamicIndexBuffer::Bind(ID3D11DeviceContext* Context)
{
    Context->IASetIndexBuffer(Buffer, DXGI_FORMAT_R32_UINT, 0);
}
