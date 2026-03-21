#pragma once

#include "Core/CoreTypes.h"
#include "Core/EngineTypes.h"
#include "Math/Vector.h"
#include "Math/Matrix.h"
#include "Render/Common/RenderTypes.h"
#include "Render/Resource/Shader.h"
#include "Render/Resource/Buffer.h"

// Texture Atlas UV 조정을 위해 선언한 구조체
struct FCharacterInfo {
	float u;
	float v;
	float width;
	float height;
};

// FFontVertex — 폰트 렌더링용 버텍스 (position + Color)
struct FFontVertex
{
	FVector	 position;
	FVector2 TexCoord;
};

// GPU로 전달하는 상수버퍼 구조체 (GPU Billboard)
struct FFontTransformConstants
{
	FMatrix ViewProj;
	FVector WorldPos; float _pad0;
	FVector CamRight; float _pad1;
	FVector CamUp;    float _pad2;
};

// Immutable Mesh
struct FFontMesh
{
	ID3D11Buffer* VertexBuffer = nullptr;
	ID3D11Buffer* IndexBuffer  = nullptr;
	uint32        IndexCount   = 0;

	bool IsValid() const { return VertexBuffer && IndexBuffer && IndexCount > 0; }
	void Release()
	{
		if (VertexBuffer) { VertexBuffer->Release(); VertexBuffer = nullptr; }
		if (IndexBuffer)  { IndexBuffer->Release();  IndexBuffer = nullptr;  }
		IndexCount = 0;
	}
};

// FFontBatcher — 셰이더/텍스처/샘플러/CB 공유 리소스 관리
class FFontBatcher
{
public:
	FFontBatcher() = default;
	~FFontBatcher() = default;

	// 공유 리소스 초기화 (셰이더, 텍스처, 샘플러, CB)
	void Create(ID3D11Device* Device);
	void Release();

	// 스폰 시 1회 — 문자열로 IMMUTABLE VB/IB 생성
	FFontMesh BuildMesh(ID3D11Device* Device, const FString& Text);

	// 매 프레임 — CB 갱신 + DrawIndexed
	void DrawText(ID3D11DeviceContext* Context,
		const FFontMesh& Mesh,
		const FVector&   WorldPos,
		const FVector&   CamRight,
		const FVector&   CamUp,
		const FMatrix&   ViewProjection);

private:
	ID3D11Resource*           FontResource = nullptr;
	ID3D11ShaderResourceView* FontAtlasSRV = nullptr;
	ID3D11SamplerState*       SamplerState = nullptr;

	TMap<char, FCharacterInfo> CharInfoMap;
	FShader         FontShader;
	FConstantBuffer FontCB;

	void BuildCharInfoMap();
	void GetCharUV(char Ch, FVector2& OutUVMin, FVector2& OutUVMax) const;
};
