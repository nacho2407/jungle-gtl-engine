#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Shader.h"
#include "Render/Resource/VertexTypes.h"

class FD3DDevice;

// SubUV 아틀라스 내 한 프레임의 UV 정보
struct FSubUVFrameInfo
{
    float U;      // 좌상단 U (0~1)
    float V;      // 좌상단 V (0~1)
    float Width;  // 프레임 UV 너비
    float Height; // 프레임 UV 높이
};

// FSubUVBatcher — SubUV 스프라이트를 배치로 모아 1회 드로우콜로 처리
//
// 사용 흐름:
//   1) Create()      — 장치 초기화 (셰이더, 샘플러, Dynamic VB/IB). 텍스처는 로드하지 않습니다.
//   2) Clear()       — 매 프레임 시작 시 이전 스프라이트 제거
//   3) AddSprite()   — 프레임 인덱스 기반 스프라이트 쿼드 누적
//   4) Flush()       — Dynamic VB/IB 업로드 + DrawIndexed 1회 호출
//                      SRV는 ResourceManager가 소유하는 FParticleResource에서 전달받습니다.
//   5) Release()     — DX 리소스 해제
class FSubUVBatcher
{
public:
    FSubUVBatcher()  = default;
    ~FSubUVBatcher() = default;

    // 공유 리소스 초기화 (셰이더, 샘플러, Dynamic VB/IB).
    // 텍스처는 로드하지 않으며 ResourceManager가 소유합니다.
    void Create(ID3D11Device* InDevice);
    void Release();

    // 월드 좌표 위에 빌보드 스프라이트 쿼드 추가 (배치에 누적).
    // Columns / Rows — 호출 시 FParticleResource에서 전달받은 아틀라스 그리드 크기
    void AddSprite(const FVector& WorldPos,
                   const FVector& CamRight,
                   const FVector& CamUp,
				   const FVector& WorldScale,
                   uint32 FrameIndex,
                   uint32 Columns,
                   uint32 Rows,
                   float Width  = 1.0f,
                   float Height = 1.0f);

    // 이번 프레임에 누적된 스프라이트 전체 제거
    void Clear();

    // Dynamic VB/IB 업로드 + DrawIndexed 1회 호출
    // SRV — ResourceManager 소유 FParticleResource의 SRV를 전달
    void Flush(FD3DDevice& Device, ID3D11DeviceContext* Context, ID3D11ShaderResourceView* SRV);

    uint32 GetSpriteCount() const { return static_cast<uint32>(Vertices.size() / 4); }

private:
    TArray<FTextureVertex> Vertices;
    TArray<uint32>         Indices;

    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer  = nullptr;

    uint32 MaxVertexCount = 0;
    uint32 MaxIndexCount  = 0;

    ID3D11Device*       Device       = nullptr; // 소유권 없음
    ID3D11SamplerState* SamplerState = nullptr;
    FShader             SubUVShader;

    void CreateBuffers();
    FSubUVFrameInfo GetFrameUV(uint32 FrameIndex, uint32 Columns, uint32 Rows) const;
};
