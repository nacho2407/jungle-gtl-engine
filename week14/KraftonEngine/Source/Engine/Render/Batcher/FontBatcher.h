#pragma once

#include "BatcherBase.h"
#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Types/RenderTypes.h"
#include "Render/Types/VertexTypes.h"

// Texture Atlas UV 정보
struct FCharacterInfo
{
	float U;
	float V;
	float Width;
	float Height;
};

// FFontBatcher — 텍스트를 배치로 모아 1회 드로우콜로 처리
//
// 사용 흐름:
//   1) Create()   — 장치 초기화 (셰이더, 샘플러, Dynamic VB/IB). 텍스처는 로드하지 않습니다.
//   2) Clear()    — 매 프레임 시작 시 이전 텍스트 제거
//   3) AddText()  — 문자별 쿼드 누적
//   4) DrawBatch()— Dynamic VB/IB 업로드 + DrawIndexed 1회 호출
//                   SRV는 ResourceManager가 소유하는 FFontResource에서 전달받습니다.
//   5) Release()  — DX 리소스 해제
class FFontBatcher : public FBatcherBase
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	// 월드 좌표 위에 빌보드 텍스트 추가 (배치에 누적)
	void AddText(const FString& Text,
		const FVector& WorldPos,
		const FVector& CamRight,
		const FVector& CamUp,
		const FVector& WorldScale,
		float Scale = 1.0f);

	// 오버레이 스탯 렌더링용
	void AddScreenText(const FString& Text,
		float ScreenX, float ScreenY,
		float ViewportWidth, float ViewportHeight,
		float Scale = 1.0f
	);

	// 이번 프레임 누적 텍스트 초기화
	void Clear();        // 빌보드 텍스트 (Vertices/Indices)
	void ClearScreen();  // 오버레이 텍스트 (ScreenVertices/ScreenIndices)

	// Dynamic VB 업로드 + 드로우콜 1회 (레거시)
	void DrawBatch(ID3D11DeviceContext* Context, const FFontResource* Resource);
	void DrawScreenBatch(ID3D11DeviceContext* Context, const FFontResource* Resource);

	// Phase 3: 버퍼 업로드 + 접근자 (FDrawCommand 경로)
	bool UploadWorldBuffers(ID3D11DeviceContext* Context);
	bool UploadScreenBuffers(ID3D11DeviceContext* Context);

	ID3D11Buffer* GetWorldVBBuffer() const;
	uint32 GetWorldVBStride() const;
	ID3D11Buffer* GetWorldIBBuffer() const;
	uint32 GetWorldIndexCount() const { return static_cast<uint32>(Indices.size()); }

	ID3D11Buffer* GetScreenVBBuffer() const;
	uint32 GetScreenVBStride() const;
	ID3D11Buffer* GetScreenIBBuffer() const;
	uint32 GetScreenIndexCount() const { return static_cast<uint32>(ScreenIndices.size()); }

	ID3D11SamplerState* GetSampler() const { return SamplerState; }

	uint32 GetQuadCount() const { return static_cast<uint32>(Vertices.size() / 4); }
	uint32 GetScreenQuadCount() const { return static_cast<uint32>(ScreenVertices.size() / 4); }

	// Phase 3: DrawBatch 경로를 거치지 않으므로 CharInfoMap 보장용
	void EnsureCharInfoMap(const FFontResource* Resource);

private:
	// CPU 누적 배열
	TArray<FTextureVertex> Vertices;
	TArray<uint32>         Indices;

	TArray<FTextureVertex> ScreenVertices;
	TArray<uint32>         ScreenIndices;

	// 스크린 전용 Dynamic VB/IB (월드 VB/IB는 BatcherBase에서 상속)
	FDynamicVertexBuffer ScreenVB_Buf;
	FDynamicIndexBuffer  ScreenIB_Buf;

	// 고유 리소스
	ID3D11SamplerState* SamplerState = nullptr;

	// CharInfoMap — Atlas 그리드가 바뀔 때만 재빌드
	TMap<uint32, FCharacterInfo> CharInfoMap;
	uint32 CachedColumns = 0;
	uint32 CachedRows    = 0;

	void BuildCharInfoMap(uint32 Columns, uint32 Rows);
	void GetCharUV(uint32 Codepoint, FVector2& OutUVMin, FVector2& OutUVMax) const;
};
