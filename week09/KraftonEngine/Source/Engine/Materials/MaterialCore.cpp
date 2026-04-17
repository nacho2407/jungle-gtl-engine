#include "MaterialCore.h"
#include "Materials/Material.h"
#include "Serialization/Archive.h"
#include "Render/Resource/Shader.h"
#include "Texture/Texture2D.h"
#include "Engine/Runtime/Engine.h"
#include "Render/Pipeline/Renderer.h"

// ─── FMaterialTemplate ───

void FMaterialTemplate::Create(FShader* InShader)
{
	ParameterLayout = InShader->GetParameterLayout(); // 셰이더에서 리플렉션된 파라미터 레이아웃 정보 확보
	Shader = InShader;
}

bool FMaterialTemplate::GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const
{
	auto it = ParameterLayout.find(Name);
	if (it != ParameterLayout.end())
	{
		OutInfo = *(it->second);
		return true;
	}
	else
	{
		return false;
	}
}

// ─── FMaterialConstantBuffer ───

FMaterialConstantBuffer::~FMaterialConstantBuffer()
{
	Release();
}

void FMaterialConstantBuffer::Init(ID3D11Device* InDevice, uint32 InSize, uint32 InSlot)
{
	Release();

	uint32 AlignedSize = (InSize + 15) & ~15;
	GPUBuffer.Create(InDevice, AlignedSize);
	CPUData = new uint8[AlignedSize]();
	Size = AlignedSize;
	SlotIndex = InSlot;
	bDirty = true;
}

void FMaterialConstantBuffer::SetData(const void* Data, uint32 InSize, uint32 Offset)
{
	if (!CPUData || Offset + InSize > Size)
	{
		return;
	}
	memcpy(CPUData + Offset, Data, InSize);
	bDirty = true;
}

void FMaterialConstantBuffer::Upload(ID3D11DeviceContext* DeviceContext)
{
	if (!bDirty)
		return;

	GPUBuffer.Update(DeviceContext, CPUData, Size);
	bDirty = false;
}

void FMaterialConstantBuffer::Release()
{
	GPUBuffer.Release();
	delete[] CPUData;
	CPUData = nullptr;
	Size = 0;
	bDirty = false;
}
