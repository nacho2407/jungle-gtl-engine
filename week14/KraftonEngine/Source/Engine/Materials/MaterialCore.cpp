#include "Materials/MaterialCore.h"

#include "Render/Shader/Shader.h"
#include <cstring>

void FMaterialTemplate::Create(FShader* InShader)
{
	if (!InShader)
	{
		ParameterLayout.clear();
		Shader = nullptr;
		return;
	}

	ParameterLayout = InShader->GetParameterLayout();
	Shader = InShader;
}

bool FMaterialTemplate::GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const
{
	auto It = ParameterLayout.find(Name);
	if (It == ParameterLayout.end())
	{
		return false;
	}

	OutInfo = *(It->second);
	return true;
}

FMaterialConstantBuffer::~FMaterialConstantBuffer()
{
	Release();
}

void FMaterialConstantBuffer::Init(ID3D11Device* InDevice, uint32 InSize, uint32 InSlot)
{
	Release();

	uint32 AlignedSize = (InSize + 15) & ~15;
	if (InDevice)
	{
		GPUBuffer.Create(InDevice, AlignedSize, "MaterialGPUBuffer");
	}

	CPUData = new uint8[AlignedSize]();
	Size = AlignedSize;
	SlotIndex = InSlot;
	bDirty = true;
}

void FMaterialConstantBuffer::SetData(const void* Data, uint32 InSize, uint32 Offset)
{
	if (!Data || !CPUData || Offset + InSize > Size)
	{
		return;
	}

	memcpy(CPUData + Offset, Data, InSize);
	bDirty = true;
}

void FMaterialConstantBuffer::Upload(ID3D11DeviceContext* DeviceContext)
{
	if (!bDirty || !DeviceContext || !CPUData)
	{
		return;
	}

	GPUBuffer.Update(DeviceContext, CPUData, Size);
	bDirty = false;
}

void FMaterialConstantBuffer::Release()
{
	GPUBuffer.Release();
	delete[] CPUData;
	CPUData = nullptr;
	Size = 0;
	SlotIndex = 0;
	bDirty = false;
}
