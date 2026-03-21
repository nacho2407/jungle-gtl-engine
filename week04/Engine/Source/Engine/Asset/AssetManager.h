#pragma once
#include <dxgiformat.h>

#include "CoreUObject/Object.h"

class IAssetLoader;
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
	FWString NormalizedPath;			// 캐시의 1차 키 Source 조회용
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
	FWString NormalizedPath;
	uint64 BuildSignature = 0; // 최종 asset/resource 재사용용
};

inline bool operator==(const FAssetKey& Lhs, const FAssetKey& Rhs)
{
	return Lhs.Type == Rhs.Type
		&& Lhs.NormalizedPath == Rhs.NormalizedPath
		&& Lhs.BuildSignature == Rhs.BuildSignature;
}

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
	const FSourceRecord* GetOrLoad(const FWString& Path);
	void Invalidate(const FWString& Path);
	void Clear();

private:
	// 파일을 실제로 읽어서 record를 완성하는 역할만 하는 함수
	bool BuildSourceRecord(const FWString& NormalizedPath, FSourceRecord& OutRecord);
	bool HasFileChanged(const FSourceRecord& Record, uint64 CurrentFileSize, uint64 CurrentWriteTimeTicks) const;
	FWString NormalizePath(const FWString& Path) const;

private:
	TMap<FWString, FSourceRecord> Records;
};

struct FAssetKeyHasher
{
	size_t operator()(const FAssetKey& Key) const noexcept
	{
		const size_t H1 = std::hash<int>{}(static_cast<int>(Key.Type));
		const size_t H2 = std::hash<FWString>{}(Key.NormalizedPath);
		const size_t H3 = std::hash<uint64>{}(Key.BuildSignature);

		size_t Result = H1;
		Result ^= H2 + 0x9e3779b9 + (Result << 6) + (Result >> 2);
		Result ^= H3 + 0x9e3779b9 + (Result << 6) + (Result >> 2);
		return Result;
	}
};

/*
 * AssetManager 의 담당
 * 1. Loader 등록
 * 2. Source Cache 사용
 * 3. Loaded asset cache 조회
 * 4. Loader에게 실제 로드 위임
 */

class UAssetManager : public UObject
{
public:
	void RegisterLoader(IAssetLoader* Loader);
	UAsset* Load(const FWString& Path, const FAssetLoadParams& Params = {});
	void Invalidate(const FWString& Path);
	void Clear();

private:
	IAssetLoader* FindLoader(const FWString& Path, const FAssetLoadParams& Params) const;
	FAssetKey MakeAssetKey(const FSourceRecord& Source, const IAssetLoader& Loader, const FAssetLoadParams& Params) const;

private:
	FSourceCache SourceCache;
	TArray<IAssetLoader*> Loaders;
	TMap<FAssetKey, UAsset*, FAssetKeyHasher> LoadedAssets;
};
