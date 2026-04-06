#include "Renderer/ObjectUniformStream.h"

#include <algorithm>
#include <cstring>

namespace
{
	constexpr UINT GConstantBufferOffsetAlignmentInConstants = 16;
	constexpr uint32 GStaticObjectUniformAllocationFlag = 0x80000000u;
	constexpr uint32 GObjectUniformAllocationIndexMask = 0x7fffffffu;

	uint32 AlignObjectCount(uint32 InObjectCount)
	{
		uint32 Capacity = 1;
		while (Capacity < InObjectCount)
		{
			Capacity <<= 1;
		}
		return Capacity;
	}

	bool IsStaticAllocation(uint32 AllocationIndex)
	{
		return (AllocationIndex & GStaticObjectUniformAllocationFlag) != 0;
	}

	uint32 EncodeStaticAllocationIndex(uint32 AllocationIndex)
	{
		return AllocationIndex | GStaticObjectUniformAllocationFlag;
	}

	uint32 DecodeAllocationIndex(uint32 AllocationIndex)
	{
		return AllocationIndex & GObjectUniformAllocationIndexMask;
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
	PendingObjects.reserve(CapacityInObjects);

	return EnsureCapacity(CapacityInObjects);
}

void FObjectUniformStream::Release()
{
	PendingObjects.clear();
	StaticObjects.clear();
	StaticAllocationByKey.clear();
	CapacityInObjects = 0;
	StaticCapacityInObjects = 0;
	bStaticBufferDirty = false;

	if (ObjectRingBuffer)
	{
		ObjectRingBuffer->Release();
		ObjectRingBuffer = nullptr;
	}

	if (StaticObjectBuffer)
	{
		StaticObjectBuffer->Release();
		StaticObjectBuffer = nullptr;
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

uint32 FObjectUniformStream::AcquireStaticWorldMatrix(uint32 ObjectKey, const FMatrix& WorldMatrix)
{
	FObjectUniformEntry Entry = {};
	Entry.ObjectConstants.World = WorldMatrix.GetTransposed();

	const auto ExistingIt = StaticAllocationByKey.find(ObjectKey);
	if (ExistingIt != StaticAllocationByKey.end())
	{
		const uint32 StaticIndex = ExistingIt->second;
		if (StaticIndex < StaticObjects.size() && StaticObjects[StaticIndex].ObjectConstants.World != Entry.ObjectConstants.World)
		{
			StaticObjects[StaticIndex] = Entry;
			bStaticBufferDirty = true;
		}
		return EncodeStaticAllocationIndex(StaticIndex);
	}

	StaticObjects.push_back(Entry);
	const uint32 StaticIndex = static_cast<uint32>(StaticObjects.size() - 1);
	StaticAllocationByKey.emplace(ObjectKey, StaticIndex);
	bStaticBufferDirty = true;
	return EncodeStaticAllocationIndex(StaticIndex);
}

bool FObjectUniformStream::UploadFrame()
{
	if (!DeviceContext1)
	{
		return true;
	}

	if (bStaticBufferDirty && !StaticObjects.empty())
	{
		if (!EnsureStaticCapacity(static_cast<uint32>(StaticObjects.size())) || !StaticObjectBuffer)
		{
			return false;
		}

		D3D11_MAPPED_SUBRESOURCE StaticMapped = {};
		if (FAILED(DeviceContext->Map(StaticObjectBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &StaticMapped)))
		{
			return false;
		}

		memcpy(StaticMapped.pData, StaticObjects.data(), StaticObjects.size() * sizeof(FObjectUniformEntry));
		DeviceContext->Unmap(StaticObjectBuffer, 0);
		bStaticBufferDirty = false;
	}

	if (PendingObjects.empty())
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
	const bool bStaticAllocation = IsStaticAllocation(AllocationIndex);
	const uint32 DecodedAllocationIndex = DecodeAllocationIndex(AllocationIndex);

	if (bStaticAllocation)
	{
		if (DecodedAllocationIndex >= StaticObjects.size())
		{
			return;
		}
	}
	else if (DecodedAllocationIndex >= PendingObjects.size())
	{
		return;
	}

	if (DeviceContext1)
	{
		ID3D11Buffer* Buffer = bStaticAllocation ? StaticObjectBuffer : ObjectRingBuffer;
		if (!Buffer)
		{
			return;
		}

		const UINT FirstConstant = DecodedAllocationIndex * GConstantBufferOffsetAlignmentInConstants;
		const UINT NumConstants = GConstantBufferOffsetAlignmentInConstants;
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
		const FObjectConstantBuffer& ObjectConstants = bStaticAllocation
			? StaticObjects[DecodedAllocationIndex].ObjectConstants
			: PendingObjects[DecodedAllocationIndex].ObjectConstants;
		memcpy(Mapped.pData, &ObjectConstants, sizeof(FObjectConstantBuffer));
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
	PendingObjects.reserve(CapacityInObjects);

	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.ByteWidth = CapacityInObjects * sizeof(FObjectUniformEntry);
	Desc.ByteWidth = (Desc.ByteWidth + 15u) & ~15u;

	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &ObjectRingBuffer));
}

bool FObjectUniformStream::EnsureStaticCapacity(uint32 InAllocationCount)
{
	if (!DeviceContext1)
	{
		return true;
	}

	const uint32 DesiredCapacity = (std::max)(AlignObjectCount(InAllocationCount), 1u);
	if (StaticObjectBuffer && StaticCapacityInObjects >= DesiredCapacity)
	{
		return true;
	}

	if (StaticObjectBuffer)
	{
		StaticObjectBuffer->Release();
		StaticObjectBuffer = nullptr;
	}

	StaticCapacityInObjects = DesiredCapacity;
	StaticObjects.reserve(StaticCapacityInObjects);

	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DYNAMIC;
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	Desc.ByteWidth = StaticCapacityInObjects * sizeof(FObjectUniformEntry);
	Desc.ByteWidth = (Desc.ByteWidth + 15u) & ~15u;

	return SUCCEEDED(Device->CreateBuffer(&Desc, nullptr, &StaticObjectBuffer));
}

