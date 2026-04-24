// 머티리얼 영역에서 공유되는 타입과 인터페이스를 정의합니다.
#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

#include <memory>

// Describes where one material parameter lives inside a reflected cbuffer.
struct FMaterialParameterInfo
{
    FString BufferName;
    uint32 SlotIndex = 0;
    uint32 Offset = 0;
    uint32 Size = 0;
    uint32 BufferSize = 0;
};

// FMaterialTemplate는 머티리얼 파라미터와 렌더 리소스를 다룹니다.
class FMaterialTemplate
{
private:
    uint32 MaterialTemplateID = 0;
    TMap<FString, FMaterialParameterInfo*> ParameterLayout;
    TArray<std::unique_ptr<FMaterialParameterInfo>> OwnedParameterLayout;

public:
    const TMap<FString, FMaterialParameterInfo*>& GetParameterInfo() const { return ParameterLayout; }
    void CreateSurfaceMaterialLayout();

    bool GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const;
};

// CPU-side storage and GPU constant buffer for one material cbuffer slot.
struct FMaterialConstantBuffer
{
    uint8* CPUData = nullptr;
    FConstantBuffer GPUBuffer;
    uint32 Size = 0;
    UINT SlotIndex = 0;
    bool bDirty = false;

    FMaterialConstantBuffer() = default;
    ~FMaterialConstantBuffer();

    FMaterialConstantBuffer(const FMaterialConstantBuffer&) = delete;
    FMaterialConstantBuffer& operator=(const FMaterialConstantBuffer&) = delete;

    void Init(ID3D11Device* InDevice, uint32 InSize, uint32 InSlot);
    void SetData(const void* Data, uint32 InSize, uint32 Offset = 0);
    void Upload(ID3D11DeviceContext* DeviceContext);
    void Release();

    FConstantBuffer* GetConstantBuffer() { return &GPUBuffer; }
};
