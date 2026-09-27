#include "BatcherBase.h"

void FBatcherBase::CreateBuffers(ID3D11Device* InDevice, uint32 VBInitCount, uint32 VBStride, uint32 IBInitCount)
{
	ReleaseBuffers();

	Device = InDevice;
	if (!Device) return;
	Device->AddRef();

	VB.Create(Device, VBInitCount, VBStride);
	IB.Create(Device, IBInitCount);
}

void FBatcherBase::ReleaseBuffers()
{
	VB.Release();
	IB.Release();

	if (Device) { Device->Release(); Device = nullptr; }
}
