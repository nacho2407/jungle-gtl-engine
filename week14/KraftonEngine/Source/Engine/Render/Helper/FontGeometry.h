#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Core/ResourceTypes.h"
#include "Math/Vector.h"
#include "Render/Types/VertexTypes.h"
#include "Render/Resource/Buffer.h"

// Texture Atlas UV 정보
struct FCharacterInfo
{
	float U;
	float V;
	float Width;
	float Height;
};

// FFontGeometry — 동적 VB/IB와 문자 지오메트리 생성을 직접 소유.
class FFontGeometry
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

	void Clear();
	void ClearScreen();

	void EnsureCharInfoMap(const FFontResource* Resource);

	bool UploadWorldBuffers(ID3D11DeviceContext* Context);
	bool UploadScreenBuffers(ID3D11DeviceContext* Context);

	ID3D11Buffer* GetWorldVBBuffer() const { return WorldVB.GetBuffer(); }
	uint32 GetWorldVBStride() const { return WorldVB.GetStride(); }
	ID3D11Buffer* GetWorldIBBuffer() const { return WorldIB.GetBuffer(); }
	uint32 GetWorldIndexCount() const { return static_cast<uint32>(WorldIndices.size()); }

	ID3D11Buffer* GetScreenVBBuffer() const { return ScreenVB.GetBuffer(); }
	uint32 GetScreenVBStride() const { return ScreenVB.GetStride(); }
	ID3D11Buffer* GetScreenIBBuffer() const { return ScreenIB.GetBuffer(); }
	uint32 GetScreenIndexCount() const { return static_cast<uint32>(ScreenIndices.size()); }

	uint32 GetWorldQuadCount() const { return static_cast<uint32>(WorldVertices.size() / 4); }
	uint32 GetScreenQuadCount() const { return static_cast<uint32>(ScreenVertices.size() / 4); }

private:
	void BuildCharInfoMap(uint32 Columns, uint32 Rows);
	void GetCharUV(uint32 Codepoint, FVector2& OutUVMin, FVector2& OutUVMax) const;

	// CPU 누적 배열
	TArray<FTextureVertex> WorldVertices;
	TArray<uint32>         WorldIndices;
	TArray<FTextureVertex> ScreenVertices;
	TArray<uint32>         ScreenIndices;

	// GPU Dynamic Buffers
	FDynamicVertexBuffer WorldVB;
	FDynamicIndexBuffer  WorldIB;
	FDynamicVertexBuffer ScreenVB;
	FDynamicIndexBuffer  ScreenIB;

	// Device
	ID3D11Device* Device = nullptr;

	// CharInfoMap
	TMap<uint32, FCharacterInfo> CharInfoMap;
	uint32 CachedColumns = 0;
	uint32 CachedRows    = 0;
};
