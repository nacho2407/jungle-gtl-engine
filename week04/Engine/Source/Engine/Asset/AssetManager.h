#pragma once
#include <dxgiformat.h>

#include "CoreUObject/Object.h"

class UAsset;

enum class EAssetType : uint8
{
	Unknown = 0,
	Texture,
	Mesh,
	Shader,
	Material,
};

struct FSourceRecord
{
	WFString NormalizedPath;			// 캐시의 1차 키 Source 조회용
	FString SourceHash;					// 진짜 내용 기반 재사용 키 decode 재사용용
	uint64 FileSize = 0;				// 빠른 변경 감지용
	uint64 LastWriteTimeTicks = 0;		// 빠른 변경 감지용
	TArray<uint8> FileBytes;			// 실제 읽어온 원본 파일 바이트
	bool bFileBytesLoaded = false;		// 파일을 이미 읽었는지 표시
};

struct FAssetKey
{
	// LoadedAssets 캐시용 키
	// - 같은 파일이라도 텍스처 설정이 다를 때
	// 서로 다른 Asset으로 취급할 수 있게 BuildSignature 를 두는 것.
	EAssetType Type = EAssetType::Unknown;
	WFString NormalizedPath;
	uint64 BuildSignature = 0; // 최종 asset/resource 재사용용
};

struct FTextureBuildSettings
{
	bool bSRGB = true;
	bool bGenerateMips = false;
	bool bIsNormalMap = false;
	DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	// adress mode, filter 같은 sampler 성격은 texture build key 보다는
	// 추후 material / sampler state 쪽으로 간다.
};

struct FTextureBuildKey	// 최종 asset/resource 재사용용
{
	FString SourceHash;
	FTextureBuildSettings Settings;
};

struct FAssetLoadParams
{
	EAssetType ExplicitType = EAssetType::Unknown;
	bool bForceReload = false;
};

class FSourceCache
{
public:
	const FSourceRecord* GetOrLoad(const WFString& Path);
	void Invalidate(const WFString& Path);
	void Clear();

private:
	bool BuildSourceRecord(const WFString& NormalizedPath, FSourceRecord& OutRecord);
	bool HasFileChanged(const FSourceRecord& Record, uint64 CurrentFileSize, uint64 CurrentWriteTimeTicks) const;
	WFString NormalizePath(const WFString& Path) const;

private:
	TMap<WFString, FSourceRecord> Records;
};

class UAssetManager : public UObject
{
public:

};

