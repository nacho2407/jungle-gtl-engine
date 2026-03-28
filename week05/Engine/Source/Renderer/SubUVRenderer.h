#pragma once

#include "CoreMinimal.h"
#include "Renderer/TextureVertex.h"
#include <d3d11.h>
#include <memory>

class FVertexShader;
class FPixelShader;
class FMaterial;
struct FMeshData;
class FRenderer;

/**
 * 스프라이트 시트 애니메이션(SubUV)을 위한 메시 생성 및 머티리얼을 관리함
 * 렌더러가 직접 그리는 대신 메시와 머티리얼 데이터를 생성하여 통합 패스에서 처리함
 */
class ENGINE_API FSubUVRenderer
{
public:
	FSubUVRenderer() = default;
	~FSubUVRenderer();

	bool Initialize(FRenderer* InRenderer, const std::wstring& TexturePath);
	void Release();

	/** SubUV 전용 머티리얼 반환 */
	FMaterial* GetSubUVMaterial() const { return SubUVMaterial.get(); }

	/** SubUV용 사각형 메시 데이터 빌드 */
	bool BuildSubUVMesh(const FVector2& Size, FMeshData& OutMesh) const;

	/** 애니메이션 프레임에 따른 UV 파라미터 업데이트 */
	void UpdateAnimationParams(
		int32 Columns, int32 Rows, int32 TotalFrames,
		int32 FirstFrame, int32 LastFrame,
		float FPS, float ElapsedTime, bool bLoop);

	/** SubUV 텍스처 및 샘플러 리소스 반환 */
	ID3D11ShaderResourceView* GetTextureSRV() const { return TextureSRV; }
	ID3D11SamplerState* GetSamplerState() const { return SamplerState; }

private:
	/** SubUV 전용 머티리얼 생성 및 설정 */
	bool CreateSubUVMaterial();

private:
	ID3D11Device* Device = nullptr;
	ID3D11DeviceContext* DeviceContext = nullptr;

	ID3D11ShaderResourceView* TextureSRV = nullptr;
	ID3D11SamplerState* SamplerState = nullptr;

	std::shared_ptr<FMaterial> SubUVMaterial;
};
