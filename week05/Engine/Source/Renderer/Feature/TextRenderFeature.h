#pragma once

#include "CoreMinimal.h"
#include "Renderer/RenderFeatureInterfaces.h"
#include "Renderer/TextMeshBuilder.h"

class FRenderer;

class ENGINE_API FTextRenderFeature final : public ISceneTextFeature
{
public:
	// 폰트 아틀라스와 기본 텍스트 머티리얼 상태를 초기화한다.
	bool Initialize(FRenderer& Renderer);
	// 텍스트 기능이 소유한 GPU 자원을 해제한다.
	void Release();

	// 텍스트 렌더링에 공통으로 쓰는 기본 머티리얼을 반환한다.
	FMaterial* GetBaseMaterial() const override;
	// 문자열 하나에 대한 글리프 메시를 생성한다.
	bool BuildMesh(
		const FString& Text,
		FRenderMesh& OutMesh,
		float LetterSpacing,
		EHorizTextAligment HorizAlignment = EHorizTextAligment::EHTA_Center,
		EVerticalTextAligment VertAlignment = EVerticalTextAligment::EVRTA_TextBottom) const override;

private:
	FTextMeshBuilder TextMeshBuilder;
};
