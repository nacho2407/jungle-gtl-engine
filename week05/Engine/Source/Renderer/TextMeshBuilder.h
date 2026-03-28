#pragma once

#include "CoreMinimal.h"
#include "Renderer/FontAtlas.h"
#include "Renderer/TextureVertex.h"
#include <d3d11.h>
#include <memory>

class FVertexShader;
class FPixelShader;
class FMaterial;
struct FMeshData;
class FRenderer;
class FRenderStateManager;
/**
 * 문자열을 렌더링 가능한 메시 데이터로 변환하고 폰트 머티리얼을 관리함
 * 렌더러가 직접 문자열을 그리는 대신 메시와 머티리얼을 생성하여 통합 패스에서 처리함
 */
class ENGINE_API FTextMeshBuilder
{
public:
	FTextMeshBuilder() = default;
	~FTextMeshBuilder();

	bool Initialize(FRenderer* InRenderer);
	void Release();

	/** 폰트 셰이더와 아틀라스가 설정된 공유 머티리얼 반환 */
	FMaterial* GetFontMaterial() const { return FontMaterial.get(); }

	/** 
	 * 문자열을 분석하여 메시 데이터(정점/인덱스)를 생성함
	 * 결과는 텍스트가 바뀔 때만 호출하여 성능을 최적화할 것을 권장함
	 */
	bool BuildTextMesh(const FString& Text, FMeshData& OutMesh) const;
	void SetFillMode(D3D11_FILL_MODE InFillMode);
	/** 폰트 아틀라스 텍스처 SRV 및 샘플러 반환 */
	ID3D11ShaderResourceView* GetAtlasSRV() const { return Atlas.GetTextureSRV(); }
	ID3D11SamplerState* GetAtlasSampler() const { return Atlas.GetSamplerState(); }

private:
	/** 폰트 렌더링용 전용 머티리얼 생성 및 설정 */
	bool CreateFontMaterial();

	/** UTF-8 문자열을 코드포인트 배열로 변환 */
	TArray<uint32> DecodeToCodepoints(const FString& Text) const;

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;
	FRenderStateManager* RenderStateManager = nullptr;
	FFontAtlas Atlas;
	std::shared_ptr<FMaterial> FontMaterial;
};