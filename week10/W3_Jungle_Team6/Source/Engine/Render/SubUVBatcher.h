#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Shader.h"
#include "Render/Resource/VertexTypes.h"

// SubUV 아틀라스 내 한 프레임의 UV 정보
struct FSubUVFrameInfo
{
    float U;      // 좌상단 U (0~1)
    float V;      // 좌상단 V (0~1)
    float Width;  // 프레임 UV 너비
    float Height; // 프레임 UV 높이
};

// FSubUVBatcher — 동일한 스프라이트 아틀라스를 사용하는 SubUV컴포넌트를 배치로 모아 1회 드로우콜로 처리
//
// 사용 흐름:
//   1) Create()      — 장치 초기화, DDS 아틀라스 로드, 그리드 크기(Columns × Rows) 지정
//   2) Clear()       — 매 프레임 시작 시 이전 스프라이트 제거
//   3) AddSprite()   — 프레임 인덱스 기반 스프라이트 쿼드 누적
//   4) Flush()       — Dynamic VB/IB 업로드 + DrawIndexed 1회 호출
//   5) Release()     — DX 리소스 해제
//
// 주의:
//   - 아틀라스 한 장 당 FSubUVBatcher 인스턴스 하나를 생성한다.
//   - 같은 아틀라스를 공유하는 컴포넌트끼리 AddSprite()로 누적한 뒤 Flush()를 호출한다.
//   - Flush()는 호출 시점의 IA/VS/PS 상태를 덮어쓴다. Renderer 쪽에서 블렌드 스테이트를 적절히 설정할 것.
class FSubUVBatcher
{
public:
    FSubUVBatcher()  = default;
    ~FSubUVBatcher() = default;

    // ---- 초기화 / 해제 ----

    // 공유 리소스 초기화.
    // AtlasPath : DDS 텍스처 파일 경로 (예: L"./Asset/Particle/Fire.dds")
    // InColumns : 아틀라스 가로 프레임 수
    // InRows    : 아틀라스 세로 프레임 수
    void Create(ID3D11Device* InDevice,
                const wchar_t* AtlasPath,
                uint32 InColumns,
                uint32 InRows);

    void Release();

    // ---- 스프라이트 누적 API ----

    // 월드 좌표 위에 빌보드 스프라이트 쿼드 추가 (배치에 누적).
    // WorldPos   : 쿼드 중심 월드 좌표
    // CamRight   : 카메라 Right 벡터 (CPU 빌보드 계산용)
    // CamUp      : 카메라 Up 벡터   (CPU 빌보드 계산용)
    // FrameIndex : 아틀라스 내 프레임 인덱스 (0 기반, 좌→우 / 위→아래 순)
    // Width      : 쿼드 월드 공간 너비  (기본값 1.0)
    // Height     : 쿼드 월드 공간 높이 (기본값 1.0)
    void AddSprite(const FVector& WorldPos,
                   const FVector& CamRight,
                   const FVector& CamUp,
                   uint32 FrameIndex,
                   float Width  = 1.0f,
                   float Height = 1.0f);

    // 이번 프레임에 누적된 스프라이트 전체 제거
    void Clear();

    // Dynamic VB/IB 업로드 + DrawIndexed 1회 호출
    void Flush(ID3D11DeviceContext* Context);

    // 현재 누적된 쿼드(스프라이트) 수
    uint32 GetSpriteCount() const { return static_cast<uint32>(Vertices.size() / 4); }

private:
    // CPU 누적 배열 (FTextureVertex = Position + TexCoord, 셰이더 입력 레이아웃 공용)
    TArray<FTextureVertex> Vertices;
    TArray<uint32>      Indices;

    // GPU 버퍼 (Dynamic)
    ID3D11Buffer* VertexBuffer = nullptr;
    ID3D11Buffer* IndexBuffer  = nullptr;

    uint32 MaxVertexCount = 0;
    uint32 MaxIndexCount  = 0;

    // 공유 DX 리소스
    ID3D11Device*             Device        = nullptr; // 버퍼 재할당 시 사용 (소유권 없음)
    ID3D11Resource*           AtlasResource = nullptr;
    ID3D11ShaderResourceView* AtlasSRV      = nullptr;
    ID3D11SamplerState*       SamplerState  = nullptr;

    FShader SubUVShader;

    // 아틀라스 그리드 정보
    uint32 Columns = 1;
    uint32 Rows    = 1;

    // Dynamic VB/IB 생성 (MaxVertexCount / MaxIndexCount 기준)
    void CreateBuffers();

    // 프레임 인덱스 → 아틀라스 내 UV 좌표 계산
    FSubUVFrameInfo GetFrameUV(uint32 FrameIndex) const;
};
