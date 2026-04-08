#pragma once

#include "BatcherBase.h"
#include "Core/CoreTypes.h"
#include "Math/Vector.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Batcher/SubUVBatcher.h" // FSRVBatch 재사용

// FBillboardBatcher — 단일 컬러 텍스처(quad) 빌보드 전용 배처
//
// SubUVBatcher 와 차이점:
//   - 프레임 인덱싱 / atlas Columns,Rows 가 없음 (UV는 항상 0~1 풀쿼드)
//   - ShaderBillboard 를 사용 (알파 컷오프)
//   - 추후 화면 고정 크기, depth-test off 등 빌보드 전용 옵션을 여기에 확장
class FBillboardBatcher : public FBatcherBase
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	void AddSprite(ID3D11ShaderResourceView* SRV,
		const FVector& WorldPos,
		const FVector& CamRight,
		const FVector& CamUp,
		const FVector& WorldScale,
		float Width  = 1.0f,
		float Height = 1.0f);

	void Clear();
	void DrawBatch(ID3D11DeviceContext* Context);

	uint32 GetSpriteCount() const { return static_cast<uint32>(Vertices.size() / 4); }

private:
	TArray<FTextureVertex> Vertices;
	TArray<uint32>         Indices;
	TArray<FSRVBatch>      Batches;

	ID3D11SamplerState* SamplerState = nullptr;
};
