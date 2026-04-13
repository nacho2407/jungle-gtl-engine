#pragma once

#include "Core/CoreTypes.h"
#include "Object/FName.h"
#include "Render/Common/ComPtr.h"
#include "Render/Resource/Texture.h"
#include <d3d11.h>

struct FTextureResource
{
	FName	Name;
	FString Path;

	UTexture* Texture;

	bool IsLoaded() const { return Texture != nullptr && Texture->GetSRV() != nullptr;  }
};


// Font/Particle 공통 텍스처 아틀라스 리소스.
// ResourceManager가 소유하며, 컴포넌트는 포인터로 참조만 합니다.
// Columns × Rows 그리드 정보를 함께 보유해 Batcher에서 UV 계산에 활용합니다.
struct FTextureAtlasResource 
{
	FName   Name;
	FString Path;							// Asset 상대 경로 (Resource.ini에서 로드)

	UTexture* Texture = nullptr;

	uint32 Columns = 1;						// 아틀라스 가로 프레임(셀) 수
	uint32 Rows    = 1;						// 아틀라스 세로 프레임(셀) 수

	bool IsLoaded() const { return Texture != nullptr && Texture->GetSRV() != nullptr; }
};

//	StaticMesh 리소스 정보 구조체 (ResourceManager에서 관리, ObjLoader로 전달)
//	ResourceManager는 Resource.ini에서 경로/옵션 정보를 로드하여 이 구조체에 담아 가지고 있습니다.
//	사용 계층은 ResourceManager 임 (ObjLoader는 다른 구조체로 옵션을 받아 사용)
struct FStaticMeshResource
{
	FString Name;
	FString Path;
	bool bPreload = false;
	bool bNormalizeToUnitCube = false;
};

//	ResouceManager -> ObjLoader로 전달되는 옵션 구조체
struct FStaticMeshLoadOptions
{
	bool bNormalizeToUnitCube = false;
};

// 의미론적 별칭 — 타입은 동일하지만 용도를 명시합니다.
using FFontResource     = FTextureAtlasResource;
using FParticleResource = FTextureAtlasResource;
using FMaterialResource = FTextureResource;

