// 엔진 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once
#include "Core/CoreTypes.h"

struct ID3D11Resource;

// MemoryStats는 엔진 영역의 핵심 동작을 담당합니다.
class MemoryStats
{
public:
    static void OnAllocated(uint64 Size)
    {
        TotalAllocationBytes += Size;
        TotalAllocationCount++;
    }

    static void OnDeallocated(uint64 Size)
    {
        TotalAllocationBytes -= Size;
        TotalAllocationCount--;
    }

    static void AddPixelShaderMemory(uint64 Size) { PixelShaderMemory += Size; }
    static void SubPixelShaderMemory(uint64 Size) { PixelShaderMemory -= Size; }

    static void AddVertexShaderMemory(uint64 Size) { VertexShaderMemory += Size; }
    static void SubVertexShaderMemory(uint64 Size) { VertexShaderMemory -= Size; }

    // Texture
    static void AddTextureMemory(uint64 Size) { TextureMemory += Size; }
    static void SubTextureMemory(uint64 Size) { TextureMemory -= Size; }

    // GPU Buffer
    static void AddVertexBufferMemory(uint64 Size) { VertexBufferMemory += Size; }
    static void SubVertexBufferMemory(uint64 Size) { VertexBufferMemory -= Size; }

    static void AddIndexBufferMemory(uint64 Size) { IndexBufferMemory += Size; }
    static void SubIndexBufferMemory(uint64 Size) { IndexBufferMemory -= Size; }

    // StaticMesh CPU
    static void AddStaticMeshCPUMemory(uint64 Size) { StaticMeshCPUMemory += Size; }
    static void SubStaticMeshCPUMemory(uint64 Size) { StaticMeshCPUMemory -= Size; }

    static uint64 GetTotalAllocationBytes() { return TotalAllocationBytes; }
    static uint32 GetTotalAllocationCount() { return TotalAllocationCount; }
    static uint64 GetPixelShaderMemory() { return PixelShaderMemory; }
    static uint64 GetVertexShaderMemory() { return VertexShaderMemory; }
    static uint64 GetTextureMemory() { return TextureMemory; }
    static uint64 GetVertexBufferMemory() { return VertexBufferMemory; }
    static uint64 GetIndexBufferMemory() { return IndexBufferMemory; }
    static uint64 GetStaticMeshCPUMemory() { return StaticMeshCPUMemory; }

    static uint64 CalculateTextureMemory(ID3D11Resource* Resource);

private:
    static uint64 TotalAllocationBytes;
    static uint32 TotalAllocationCount;
    static uint64 PixelShaderMemory;
    static uint64 VertexShaderMemory;
    static uint64 TextureMemory;
    static uint64 VertexBufferMemory;
    static uint64 IndexBufferMemory;
    static uint64 StaticMeshCPUMemory;
};
