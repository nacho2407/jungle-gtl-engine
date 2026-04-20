#pragma once

#include "Core/CoreTypes.h"
#include "Render/Hardware/Resources/Buffer.h"

template <typename TVertex>
struct TBatchBuffer
{
    void Create(ID3D11Device* InDevice, uint32 InitialVertexCapacity, uint32 InitialIndexCapacity)
    {
        Release();
        Device = InDevice;
        if (!Device)
            return;
        Device->AddRef();

        VertexBuffer.Create(Device, InitialVertexCapacity, sizeof(TVertex));
        IndexBuffer.Create(Device, InitialIndexCapacity);
    }

    void Release()
    {
        Clear();
        VertexBuffer.Release();
        IndexBuffer.Release();
        if (Device)
        {
            Device->Release();
            Device = nullptr;
        }
    }

    void Clear()
    {
        Vertices.clear();
        Indices.clear();
    }

    bool Upload(ID3D11DeviceContext* Context)
    {
        const uint32 VertexCount = static_cast<uint32>(Vertices.size());
        const uint32 IndexCount = static_cast<uint32>(Indices.size());
        if (!Device || !Context || VertexCount == 0 || IndexCount == 0)
            return false;

        VertexBuffer.EnsureCapacity(Device, VertexCount);
        IndexBuffer.EnsureCapacity(Device, IndexCount);
        if (!VertexBuffer.Update(Context, Vertices.data(), VertexCount))
            return false;
        if (!IndexBuffer.Update(Context, Indices.data(), IndexCount))
            return false;
        return true;
    }

    ID3D11Buffer* GetVBBuffer() const { return VertexBuffer.GetBuffer(); }
    uint32 GetVBStride() const { return VertexBuffer.GetStride(); }
    ID3D11Buffer* GetIBBuffer() const { return IndexBuffer.GetBuffer(); }
    uint32 GetIndexCount() const { return static_cast<uint32>(Indices.size()); }

    TArray<TVertex> Vertices;
    TArray<uint32> Indices;

    FDynamicVertexBuffer VertexBuffer;
    FDynamicIndexBuffer IndexBuffer;
    ID3D11Device* Device = nullptr;
};
