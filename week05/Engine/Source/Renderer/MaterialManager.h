#pragma once

#include "CoreMinimal.h"
#include <d3d11.h>
#include <memory>

class FMaterial;
class FRenderStateManager;

// Material 에셋 로더 + 캐시 (싱글톤)
// JSON 파일에서 Material을 로드하고 이름 기반으로 캐싱
class ENGINE_API FMaterialManager
{
public:
	static FMaterialManager& Get();

	// 지정된 디렉토리 내의 모든 머티리얼을 미리 로드
	void LoadAllMaterials(ID3D11Device* Device, FRenderStateManager* InStateManager);

	// JSON 파일에서 Material 로드 (캐시 히트 시 기존 반환)
	std::shared_ptr<FMaterial> LoadFromFile(ID3D11Device* InDevice, FRenderStateManager* InStateManager, const FString& InFilePath);

	// 이름으로 이미 로드된 Material 조회
	std::shared_ptr<FMaterial> FindByName(const FString& Name) const;

	// 프로그래밍 방식으로 생성한 Material 등록
	void Register(const FString& Name, const std::shared_ptr<FMaterial>& InMaterial);

	// 로드된 Material 파일 경로 목록 반환
	TArray<FString> GetLoadedPaths() const;

	void Clear();

private:
	FMaterialManager() = default;

	// 파일 경로 기반 캐시
	TMap<FString, std::shared_ptr<FMaterial>> PathCache;
	// 이름 기반 캐시 (JSON의 "Name" 필드)
	TMap<FString, std::shared_ptr<FMaterial>> NameCache;
};
