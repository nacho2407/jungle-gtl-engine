#pragma once

#include "Math/Vector.h"
#include "Math/Matrix.h"

#include "Render/D3D11/Buffers/Buffers.h"


class FShader;

// 파라미터 이름 → 상수 버퍼 내 위치 매핑
struct FMaterialParameterInfo
{
    FString BufferName; // ConstantBuffers 이름 "PerMaterial""PerFrame"
    uint32 SlotIndex;   // ConstantBuffers 슬롯 인덱스

    uint32 Offset; // 버퍼 내 바이트 오프셋
    uint32 Size;   // 바이트 크기

    uint32 BufferSize; // 이 변수가 속한 상수 버퍼의 전체 크기 (16의 배수)
};


// 셰이더 + 레이아웃 (불변, 공유)
// Template은 셰이더 파일이 있으면 언제든 재생성 가능
class FMaterialTemplate
{
private:
    uint32 MaterialTemplateID;                              // 고유 ID
    FShader* Shader;                                        // 어떤 셰이더를 사용하는지
    TMap<FString, FMaterialParameterInfo*> ParameterLayout; // 리플렉션 결과 : 쉐이더 constant buffer 레이아웃 정보

public:
    const TMap<FString, FMaterialParameterInfo*>& GetParameterInfo() const { return ParameterLayout; }
    void Create(FShader* InShader);

    FShader* GetShader() const { return Shader; }
    bool GetParameterInfo(const FString& Name, FMaterialParameterInfo& OutInfo) const;
};


// 실제 데이터가 올라가는 버퍼
struct FMaterialConstantBuffer
{
    uint8* CPUData; // CPU 메모리의 실제 값
    FConstantBuffer GPUBuffer;
    uint32 Size = 0;
    UINT SlotIndex = 0; // cbuffer 바인딩 슬롯 (b0, b1 등)
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
