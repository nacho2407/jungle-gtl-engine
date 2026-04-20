#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"
#include "Math/Vector.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Submission/Batching/BatchBuffer.h"

// Texture Atlas UV 정보
struct FCharacterInfo
{
	float U;
	float V;
	float Width;
	float Height;
};

// FFontBatch — 월드/스크린 텍스트 배치를 만들고 업로드한다.
class FFontBatch
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	// 월드 좌표 빌보드 텍스트
	void AddWorldText(const FString& Text,
		const FVector& WorldPos,
		const FVector& CamRight,
		const FVector& CamUp,
		const FVector& WorldScale,
		float Scale = 1.0f);

	// 스크린 공간 오버레이 텍스트
	void AddScreenText(const FString& Text,
		float ScreenX, float ScreenY,
		float ViewportWidth, float ViewportHeight,
		float Scale = 1.0f);

	void ClearWorld();
	void ClearScreen();
	void ClearAll();

	void EnsureCharInfoMap(const FFontResource* Resource);

	bool UploadWorldBuffers(ID3D11DeviceContext* Context);
	bool UploadScreenBuffers(ID3D11DeviceContext* Context);

	ID3D11Buffer* GetWorldVBBuffer() const { return WorldBuffer.GetVBBuffer(); }
	uint32 GetWorldVBStride() const { return WorldBuffer.GetVBStride(); }
	ID3D11Buffer* GetWorldIBBuffer() const { return WorldBuffer.GetIBBuffer(); }
	uint32 GetWorldIndexCount() const { return WorldBuffer.GetIndexCount(); }

	ID3D11Buffer* GetScreenVBBuffer() const { return ScreenBuffer.GetVBBuffer(); }
	uint32 GetScreenVBStride() const { return ScreenBuffer.GetVBStride(); }
	ID3D11Buffer* GetScreenIBBuffer() const { return ScreenBuffer.GetIBBuffer(); }
	uint32 GetScreenIndexCount() const { return ScreenBuffer.GetIndexCount(); }

	uint32 GetWorldQuadCount() const { return static_cast<uint32>(WorldBuffer.Vertices.size() / 4); }
	uint32 GetScreenQuadCount() const { return static_cast<uint32>(ScreenBuffer.Vertices.size() / 4); }

private:
	void BuildCharInfoMap(uint32 Columns, uint32 Rows);
	void GetCharUV(uint32 Codepoint, FVector2& OutUVMin, FVector2& OutUVMax) const;

	TBatchBuffer<FTextureVertex> WorldBuffer;
	TBatchBuffer<FTextureVertex> ScreenBuffer;

	TMap<uint32, FCharacterInfo> CharInfoMap;
	uint32 CachedColumns = 0;
	uint32 CachedRows    = 0;
};
