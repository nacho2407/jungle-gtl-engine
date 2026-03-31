#pragma once
#include "Core/CoreTypes.h"

class MemoryStats
{
public:
	static void OnAllocated(uint32 Size)
	{
		TotalAllocationBytes += Size;
		TotalAllocationCount++;
	}

	static void OnDeallocated(uint32 Size)
	{
		TotalAllocationBytes -= Size;
		TotalAllocationCount--;
	}

	static void AddPixelShaderMemory(uint32 Size) { PixelShaderMemory += Size; }
	static void SubPixelShaderMemory(uint32 Size) { PixelShaderMemory -= Size; }

	static void AddVertexShaderMemory(uint32 Size) { VertexShaderMemory += Size; }
	static void SubVertexShaderMemory(uint32 Size) { VertexShaderMemory -= Size; }

	// Texture
	static void AddTextureMemory(uint32 Size) { TextureMemory += Size; }
	static void SubTextureMemory(uint32 Size) { TextureMemory -= Size; }

	// GPU Buffer
	static void AddVertexBufferMemory(uint32 Size) { VertexBufferMemory += Size; }
	static void SubVertexBufferMemory(uint32 Size) { VertexBufferMemory -= Size; }

	static void AddIndexBufferMemory(uint32 Size) { IndexBufferMemory += Size; }
	static void SubIndexBufferMemory(uint32 Size) { IndexBufferMemory -= Size; }

	// StaticMesh CPU
	static void AddStaticMeshCPUMemory(uint32 Size) { StaticMeshCPUMemory += Size; }
	static void SubStaticMeshCPUMemory(uint32 Size) { StaticMeshCPUMemory -= Size; }

	static uint32 GetTotalAllocationBytes() { return TotalAllocationBytes; }
	static uint32 GetTotalAllocationCount() { return TotalAllocationCount; }
	static uint32 GetPixelShaderMemory() { return PixelShaderMemory; }
	static uint32 GetVertexShaderMemory() { return VertexShaderMemory; }
	static uint32 GetTextureMemory() { return TextureMemory; }
	static uint32 GetVertexBufferMemory() { return VertexBufferMemory; }
	static uint32 GetIndexBufferMemory() { return IndexBufferMemory; }
	static uint32 GetStaticMeshCPUMemory() { return StaticMeshCPUMemory; }


private:
	static uint32 TotalAllocationBytes;
	static uint32 TotalAllocationCount;
	static uint32 PixelShaderMemory;
	static uint32 VertexShaderMemory;
	static uint32 TextureMemory;
	static uint32 VertexBufferMemory;
	static uint32 IndexBufferMemory;
	static uint32 StaticMeshCPUMemory;
};
