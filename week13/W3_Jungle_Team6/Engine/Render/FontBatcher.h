#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Common/RenderTypes.h"

// ============================================================
// FFontVertex — 폰트 렌더링용 버텍스 (Position + UV + Color)
// ============================================================
struct FFontVertex
{
	FVector Position;
	FVector2 TexCoord;
	FVector4 Color;
};

// ============================================================
// FFontBatcher — 텍스트를 빌보드 Quad로 모아서 한 번에 그리는 배처
//
// 사용 흐름:
//   1) Clear()         — 매 프레임 시작 시 이전 텍스트 제거
//   2) AddText()       — 월드 좌표 + 카메라 방향으로 빌보드 텍스트 추가
//   3) Flush()         — Dynamic VB 업로드 + Draw Call
//
// 파트 C가 구현, 파트 A(Renderer)가 매 프레임 호출
// ============================================================
class FFontBatcher
{
public:
	FFontBatcher() = default;
	~FFontBatcher() = default;

	// ---- 초기화 / 해제 ----

	// 폰트 아틀라스 텍스쳐 로드 + SRV 생성 + Dynamic VB 할당
	void Create(ID3D11Device* Device);
	void Release();

	// ---- 텍스트 축적 API ----

	// 월드 좌표(WorldPos) 위에 빌보드 텍스트 1개 추가
	// CamRight, CamUp: 카메라의 Right/Up 벡터 (빌보드 회전에 사용)
	void AddText(const FString& Text,
				 const FVector& WorldPos,
				 const FVector& CamRight,
				 const FVector& CamUp,
				 const FColor& Color = FColor::White(),
				 float Scale = 1.0f);

	// 이번 프레임에 축적된 텍스트 모두 제거
	void Clear();

	// ---- 렌더링 ----

	// Dynamic VB 업로드 + 아틀라스 텍스쳐 바인딩 + Draw Call
	// ViewProj: 카메라의 View * Projection 행렬
	void Flush(ID3D11DeviceContext* Context, const FMatrix& ViewProj);

	// 현재 축적된 Quad(문자) 개수
	uint32 GetQuadCount() const;

private:
	TArray<FFontVertex> Vertices;
	TArray<uint32> Indices;

	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer = nullptr;
	ID3D11ShaderResourceView* FontAtlasSRV = nullptr;
	ID3D11SamplerState* SamplerState = nullptr;

	uint32 MaxVertexCount = 0;
	uint32 MaxIndexCount = 0;

	// 폰트 아틀라스 내 문자별 UV 좌표 계산
	void GetCharUV(char Ch, FVector2& OutUVMin, FVector2& OutUVMax) const;
};
