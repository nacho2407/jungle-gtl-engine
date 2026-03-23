#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Object/FName.h"

#include <d3d11.h>

enum class EResourceType
{
	Font,
	Particle
};
// Font Atlas 리소스. ResourceManager가 소유하며, 컴포넌트는 포인터로 참조.
struct FFontResource
{
	EResourceType ResourceType;			// Resource의 종류가 무엇인지.
	FName Name;
	FString Path;						// Asset 상대 경로 (Resource.ini에서 로드)

	ID3D11ShaderResourceView* SRV = nullptr;	// GPU에 로드된 Font Atlas 텍스처

	bool IsLoaded() const { return SRV != nullptr; }
};

// 리소스를 관리하는 싱글턴.
// Resource.ini에서 리소스 경로를 읽고, GPU 리소스를 로드/캐싱합니다.
// 컴포넌트는 소유하지 않고 포인터로 공유 데이터를 바라봅니다.
//
// Resource.ini 예시:
// {
//     "Font": {
//         "Default": "Asset/Font/DefaultFont.png"
//     }
// }
class FResourceManager : public TSingleton<FResourceManager>
{
	friend class TSingleton<FResourceManager>;

public:
	// Resource.ini에서 경로 정보 로드
	void LoadFromFile(const FString& Path, ID3D11Device* InDevice);

	// GPU 리소스 로드 (Device 필요, 초기화 후 호출)
	bool LoadGPUResources(ID3D11Device* Device);

	// 모든 GPU 리소스 해제
	void ReleaseGPUResources();

	// --- Font ---
	// Font 이름으로 리소스 조회 (없으면 nullptr)
	FFontResource* FindFont(const FName& FontName);
	const FFontResource* FindFont(const FName& FontName) const;

	// Font 리소스 등록 (런타임 추가용)
	void RegisterFont(const FName& FontName, const FString& InPath);

private:
	FResourceManager() = default;
	~FResourceManager() { ReleaseGPUResources(); }

	TMap<FString, FFontResource> Resources;
};
