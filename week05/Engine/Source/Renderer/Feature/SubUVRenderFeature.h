#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderFeatureInterfaces.h"
#include "Renderer/SubUVRenderer.h"

class FRenderer;

class ENGINE_API FSubUVRenderFeature final : public ISceneSubUVFeature
{
public:
	// 공용 SubUV 머티리얼과 소스 텍스처를 초기화한다.
	bool Initialize(FRenderer& Renderer, const std::wstring& TexturePath);
	// SubUV 기능이 소유한 GPU 자원을 해제한다.
	void Release();

	// SubUV 스프라이트가 공통으로 쓰는 기본 머티리얼을 반환한다.
	FMaterial* GetBaseMaterial() const override;
	// SubUV 스프라이트용 빌보드 쿼드 메시를 생성한다.
	bool BuildMesh(const FVector2& Size, FRenderMesh& OutMesh) const override;

private:
	FSubUVRenderer SubUVRenderer;
};
