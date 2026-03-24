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
//   4) Flush()    — Dynamic VB/IB 업로드 + DrawIndexed 1회 호출
//                   SRV는 ResourceManager가 소유하는 FFontResource에서 전달받습니다.
//   5) Release()  — DX 리소스 해제
class FFontBatcher
{
public:
	FFontBatcher() = default;
	~FFontBatcher() = default;

	// 공유 리소스 초기화 (셰이더, 샘플러, Dynamic VB/IB).
	// 텍스처는 로드하지 않으며 ResourceManager가 소유합니다.
	void Create(ID3D11Device* InDevice);
	void Release();

	// 월드 좌표 위에 빌보드 텍스트 추가 (배치에 누적)
	void AddText(const FString& Text,
		const FVector& WorldPos,
		const FVector& CamRight,
		const FVector& CamUp,
		const FVector& WorldScale,
		float Scale = 1.0f);

	// 이번 프레임 누적 텍스트 초기화
	void Clear();

	// Dynamic VB 업로드 + 드로우콜 1회
	// Resource — FontBatcher가 사용할 FontAtlas 리소스 (ResourceManager 소유)
	void Flush(FD3DDevice& Device, ID3D11DeviceContext* Context, const FFontResource* Resource);

	uint32 GetQuadCount() const { return static_cast<uint32>(Vertices.size() / 4); }

private:
	// CPU 누적 배열
	TArray<FTextureVertex> Vertices;
	TArray<uint32>         Indices;

	// GPU 버퍼 (Dynamic)
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer  = nullptr;

	uint32 MaxVertexCount = 0;
	uint32 MaxIndexCount  = 0;

	// 공유 DX 리소스
	ID3D11Device*       Device       = nullptr;
	ID3D11SamplerState* SamplerState = nullptr;
	FShader             FontShader;

	// CharInfoMap — Atlas 그리드가 바뀔 때만 재빌드
	// key: Unicode 코드포인트 (ASCII 33~126, 한글 U+AC00~U+D7A3)
	TMap<uint32, FCharacterInfo> CharInfoMap;
	uint32 CachedColumns = 0;
	uint32 CachedRows    = 0;

	void CreateBuffers();
	void BuildCharInfoMap(uint32 Columns, uint32 Rows);
	void GetCharUV(uint32 Codepoint, FVector2& OutUVMin, FVector2& OutUVMax) const;
};
