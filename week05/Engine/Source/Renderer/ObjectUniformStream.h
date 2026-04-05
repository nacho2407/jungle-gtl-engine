#pragma once

#include "CoreMinimal.h"
#include "Renderer/ShaderType.h"
#include <d3d11_1.h>

class ENGINE_API FObjectUniformStream
{
public:
	FObjectUniformStream() = default;
	~FObjectUniformStream();

	bool Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, ID3D11Buffer* InFallbackObjectConstantBuffer, uint32 InInitialCapacity = 2048);
	void Release();

	void Reset();
	uint32 AllocateWorldMatrix(const FMatrix& WorldMatrix);
	bool UploadFrame();
	void BindAllocation(uint32 AllocationIndex);

private:
	bool EnsureCapacity(uint32 InAllocationCount);

private:
	struct FObjectUniformEntry
	{
		FObjectConstantBuffer ObjectConstants = {};
		float Padding[48] = {};
	};
	static_assert(sizeof(FObjectUniformEntry) == 256, "Object uniform entry must be 256 bytes.");

	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	ID3D11DeviceContext1* DeviceContext1 = nullptr;
	ID3D11Buffer* FallbackObjectConstantBuffer = nullptr;
	ID3D11Buffer* ObjectRingBuffer = nullptr;
	uint32 CapacityInObjects = 0;
	TArray<FObjectUniformEntry> PendingObjects;
};
