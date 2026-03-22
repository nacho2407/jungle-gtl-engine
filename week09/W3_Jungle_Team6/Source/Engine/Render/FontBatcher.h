#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Shader.h"
#include "Render/Resource/Buffer.h"

// Texture Atlas UV 정보
struct FCharacterInfo
{
	float u;
	float v;
	float width;
	float height;
};



// FFontBatcher — 텍스트를 배치로 모아 1회 드로우콜로 처리
class FFontBatcher
{
public:
	FFontBatcher() = default;
	~FFontBatcher() = default;

	// 공유 리소스 초기화 (셰이더, 텍스처, 샘플러, Dynamic VB/IB, CB)
	void Create(ID3D11Device* InDevice);
	void Release();

	// 월드 좌표 위에 빌보드 텍스트 추가 (배치에 누적)
	void AddText(const FString& Text,
		const FVector& WorldPos,
		const FVector& CamRight,
		const FVector& CamUp,
		float Scale = 1.0f);

	// 이번 프레임 누적 텍스트 초기화
	void Clear();

	// Dynamic VB 업로드 + 드로우콜 (1회)
	void Flush(ID3D11DeviceContext* Context);

	// 현재 누적된 Quad(문자) 수
	uint32 GetQuadCount() const { return static_cast<uint32>(Vertices.size() / 4); }

private:
	// CPU 누적 배열
	TArray<FFontVertex> Vertices;
	TArray<uint32>      Indices;

	// GPU 버퍼 (Dynamic)
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer  = nullptr;

	uint32 MaxVertexCount      = 0; // 정점 버퍼에 할당 가능한 최대 정점 개수
	uint32 MaxIndexCount       = 0; // 색인 버퍼에 할당 가능한 최대 정점 개수

	// 공유 DX 리소스
	ID3D11Device*             Device       = nullptr;  // 버퍼 재할당 시 사용
	ID3D11Resource*           FontResource = nullptr;
	ID3D11ShaderResourceView* FontAtlasSRV = nullptr;
	ID3D11SamplerState*       SamplerState = nullptr;

	TMap<char, FCharacterInfo> CharInfoMap;
	FShader FontShader;

	// Dynamic VB/IB 생성 (MaxVertexCount/MaxIndexCount 기준)
	void CreateBuffers();
	// Texture Atlas Slicing
	void BuildCharInfoMap();
	// 해당 문자열을 key로 가지는 구조체의 UV값 얻는 함수
	void GetCharUV(char Ch, FVector2& OutUVMin, FVector2& OutUVMax) const;
};
