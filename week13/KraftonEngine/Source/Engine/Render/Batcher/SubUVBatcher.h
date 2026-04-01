#pragma once

#include "BatcherBase.h"
#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/VertexTypes.h"

// SubUV 아틀라스 내 한 프레임의 UV 정보
struct FSubUVFrameInfo
{
	float U;      // 좌상단 U (0~1)
	float V;      // 좌상단 V (0~1)
	float Width;  // 프레임 UV 너비
	float Height; // 프레임 UV 높이
};

struct FSRVBatch
{
	ID3D11ShaderResourceView* SRV;
	uint32 IndexStart;   // Indices 배열 내 시작 위치
	uint32 IndexCount;   // 이 SRV에 해당하는 인덱스 수
	int32  BaseVertex;   // DrawIndexed의 BaseVertexLocation
};

// FSubUVBatcher — SubUV 스프라이트를 배치로 모아 드로우콜로 처리
//
// 사용 흐름:
//   1) Create()      — 장치 초기화 (셰이더, 샘플러, Dynamic VB/IB)
//   2) Clear()       — 매 프레임 시작 시 이전 스프라이트 제거
//   3) AddSprite()   — 프레임 인덱스 기반 스프라이트 쿼드 누적
//   4) DrawBatch()   — Dynamic VB/IB 업로드 + SRV별 DrawIndexed 호출
//   5) Release()     — DX 리소스 해제
class FSubUVBatcher : public FBatcherBase
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	// 월드 좌표 위에 빌보드 스프라이트 쿼드 추가 (배치에 누적)
	void AddSprite(ID3D11ShaderResourceView* SRV,
		const FVector& WorldPos,
		const FVector& CamRight,
		const FVector& CamUp,
		const FVector& WorldScale,
		uint32 FrameIndex,
		uint32 Columns,
		uint32 Rows,
		float Width  = 1.0f,
		float Height = 1.0f);

	void Clear();

	// Dynamic VB/IB 업로드 + SRV별 DrawIndexed
	void DrawBatch(ID3D11DeviceContext* Context);

	uint32 GetSpriteCount() const { return static_cast<uint32>(Vertices.size() / 4); }

private:
	TArray<FTextureVertex> Vertices;
	TArray<uint32>         Indices;
	TArray<FSRVBatch>      Batches;

	// 고유 리소스
	ID3D11SamplerState* SamplerState = nullptr;

	FSubUVFrameInfo GetFrameUV(uint32 FrameIndex, uint32 Columns, uint32 Rows) const;
};
