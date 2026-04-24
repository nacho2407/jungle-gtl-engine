// 머티리얼 영역의 세부 동작을 구현합니다.
#include "MaterialCore.h"

#include "Materials/MaterialSemantics.h"
#include "Render/Resources/Bindings/RenderBindingSlots.h"

void FMaterialTemplate::CreateSurfaceMaterialLayout()
{
    ParameterLayout.clear();
    OwnedParameterLayout.clear();

    constexpr uint32 BufferSize = 48;
    constexpr uint32 SlotIndex = ECBSlot::PerShader0;
    const FString BufferName = "StaticMeshMaterialBuffer";

    auto AddParameter = [&](const FString& Name, uint32 Offset, uint32 Size)
    {
        auto Info = std::make_unique<FMaterialParameterInfo>();
        Info->BufferName = BufferName;
        Info->SlotIndex = SlotIndex;
        Info->Offset = Offset;
        Info->Size = Size;
        Info->BufferSize = BufferSize;
        ParameterLayout[Name] = Info.get();
        OwnedParameterLayout.push_back(std::move(Info));
    };

    AddParameter(MaterialSemantics::SectionColorParameter, 0, sizeof(float) * 4);
    AddParameter("MaterialParam", 16, sizeof(float) * 4);
    AddParameter(MaterialSemantics::SpecularPowerParameter, 16, sizeof(float));
    AddParameter(MaterialSemantics::SpecularStrengthParameter, 20, sizeof(float));
    AddParameter("HasBaseTexture", 32, sizeof(uint32));
    AddParameter("HasNormalTexture", 36, sizeof(uint32));
    AddParameter("HasSpecularTexture", 40, sizeof(uint32));
    AddParameter("StaticMeshMaterialPadding", 44, sizeof(float));
}

bool FMaterialTemplate::GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const
{
    auto It = ParameterLayout.find(Name);
    if (It == ParameterLayout.end() || !It->second)
    {
        return false;
    }

    OutInfo = *It->second;
    return true;
}

FMaterialConstantBuffer::~FMaterialConstantBuffer()
{
    Release();
}

void FMaterialConstantBuffer::Init(ID3D11Device* InDevice, uint32 InSize, uint32 InSlot)
{
    Release();

    const uint32 AlignedSize = (InSize + 15) & ~15;
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
    bDirty = false;
}
