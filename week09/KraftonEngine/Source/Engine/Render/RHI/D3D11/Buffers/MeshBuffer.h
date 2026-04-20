#pragma once
#include "Core/CoreTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/RHI/D3D11/Buffers/VertexBuffer.h"
#include "Render/RHI/D3D11/Buffers/IndexBuffer.h"
class FMeshBuffer
{
public:
    FMeshBuffer() = default;
    ~FMeshBuffer() { Release(); }
    FMeshBuffer(const FMeshBuffer&) = delete;
    FMeshBuffer& operator=(const FMeshBuffer&) = delete;
    FMeshBuffer(FMeshBuffer&&) = default;
    FMeshBuffer& operator=(FMeshBuffer&&) = default;

    template <typename VertexType>
    void Create(ID3D11Device* InDevice, const TMeshData<VertexType>& InMeshData)
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
