#include "Renderer/ObjectUniformStream.h"

#include <algorithm>
#include <cstring>

namespace
{
	constexpr UINT GConstantBufferOffsetAlignmentInConstants = 16;

	uint32 AlignObjectCount(uint32 InObjectCount)
	{
		uint32 Capacity = 1;
		while (Capacity < InObjectCount)
		{
			Capacity <<= 1;
		}
		return Capacity;
	}
}

FObjectUniformStream::~FObjectUniformStream()
{
	Release();
}

bool FObjectUniformStream::Initialize(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, ID3D11Buffer* InFallbackObjectConstantBuffer, uint32 InInitialCapacity)
{
	Release();

	Device = InDevice;
	DeviceContext = InDeviceContext;
	FallbackObjectConstantBuffer = InFallbackObjectConstantBuffer;

	if (!Device || !DeviceContext)
	{
		return false;
	}

	DeviceContext->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&DeviceContext1));
	CapacityInObjects = (std::max)(AlignObjectCount(InInitialCapacity), 1u);

	return EnsureCapacity(CapacityInObjects);
}

void FObjectUniformStream::Release()
{
	PendingObjects.clear();
	CapacityInObjects = 0;

	if (ObjectRingBuffer)
	{
		ObjectRingBuffer->Release();
		ObjectRingBuffer = nullptr;
	}

	if (DeviceContext1)
	{
		DeviceContext1->Release();
		DeviceContext1 = nullptr;
	}

	FallbackObjectConstantBuffer = nullptr;
	DeviceContext = nullptr;
	Device = nullptr;
}

void FObjectUniformStream::Reset()
{
	PendingObjects.clear();
}

uint32 FObjectUniformStream::AllocateWorldMatrix(const FMatrix& WorldMatrix)
{
	FObjectUniformEntry Entry = {};
	Entry.ObjectConstants.World = WorldMatrix.GetTransposed();
	PendingObjects.push_back(Entry);
	return static_cast<uint32>(PendingObjects.size() - 1);
}

bool FObjectUniformStream::UploadFrame()
{
	if (!DeviceContext1 || PendingObjects.empty())
	{
		return true;
	}

	if (!EnsureCapacity(static_cast<uint32>(PendingObjects.size())) || !ObjectRingBuffer)
	{
		return false;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (FAILED(DeviceContext->Map(ObjectRingBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		return false;
	}

	memcpy(Mapped.pData, PendingObjects.data(), PendingObjects.size() * sizeof(FObjectUniformEntry));
	DeviceContext->Unmap(ObjectRingBuffer, 0);
	return true;
}

void FObjectUniformStream::BindAllocation(uint32 AllocationIndex)
{
	if (AllocationIndex >= PendingObjects.size())
	{
		return;
	}

	if (DeviceContext1 && ObjectRingBuffer)
	{
		const UINT FirstConstant = AllocationIndex * GConstantBufferOffsetAlignmentInConstants;
		const UINT NumConstants = GConstantBufferOffsetAlignmentInConstants;
		ID3D11Buffer* Buffer = ObjectRingBuffer;
		DeviceContext1->VSSetConstantBuffers1(1, 1, &Buffer, &FirstConstant, &NumConstants);
		return;
	}

	if (!FallbackObjectConstantBuffer)
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE Mapped = {};
	if (SUCCEEDED(DeviceContext->Map(FallbackObjectConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped)))
	{
		memcpy(Mapped.pData, &PendingObjects[AllocationIndex].ObjectConstants, sizeof(FObjectConstantBuffer));
		DeviceContext->Unmap(FallbackObjectConstantBuffer, 0);
	}

	ID3D11Buffer* Buffer = FallbackObjectConstantBuffer;
	DeviceContext->VSSetConstantBuffers(1, 1, &Buffer);
}

bool FObjectUniformStream::EnsureCapacity(uint32 InAllocationCount)
{
	if (!DeviceContext1)
	{
		return true;
	}

	const uint32 DesiredCapacity = (std::max)(AlignObjectCount(InAllocationCount), 1u);
	if (ObjectRingBuffer && CapacityInObjects >= DesiredCapacity)
	{
		return true;
	}

	if (ObjectRingBuffer)
	{
		ObjectRingBuffer->Release();
		ObjectRingBuffer = nullptr;
	}

	CapacityInObjects = DesiredCapacity;

	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.ByteWidth = CapacityInObjects * sizeof(FObjectUniformEntry);
	Desc.ByteWidth = (Desc.ByteWidth + 15u) & ~15u;

	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &ObjectRingBuffer));
}

