#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/RHI/D3D11/Buffers/Buffers.h"

#include <memory>

class FShader;

// 머티리얼 파라미터가 어느 cbuffer/offset에 들어가는지 설명하는 레이아웃 항목입니다.
struct FMaterialParameterInfo
{
    FString BufferName;
    uint32 SlotIndex = 0;
    uint32 Offset = 0;
    uint32 Size = 0;
    uint32 BufferSize = 0;
};

// 머티리얼은 셰이더 파일을 직접 고르지 않습니다.
// 이 템플릿은 렌더러가 고정한 표준 surface cbuffer 레이아웃만 공유합니다.
class FMaterialTemplate
{
private:
    uint32 MaterialTemplateID = 0;
    FShader* Shader = nullptr;
    TMap<FString, FMaterialParameterInfo*> ParameterLayout;
    TArray<std::unique_ptr<FMaterialParameterInfo>> OwnedParameterLayout;

public:
    const TMap<FString, FMaterialParameterInfo*>& GetParameterInfo() const { return ParameterLayout; }
    void Create(FShader* InShader);
    void CreateSurfaceMaterialLayout();

    FShader* GetShader() const { return Shader; }
    bool GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const;
};

// CPU/GPU 양쪽에 존재하는 실제 머티리얼 cbuffer입니다.
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
