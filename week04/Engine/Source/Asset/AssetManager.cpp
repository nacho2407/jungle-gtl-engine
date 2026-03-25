#include "Core/CoreMinimal.h"
#include "AssetManager.h"

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>

#include "Asset.h"
#include "AssetLoader.h"

namespace
{
	namespace fs = std::filesystem;

	const char* AssetTypeToString(EAssetType Type)
	{
		switch (Type)
		{
		case EAssetType::Texture:
			return "Texture";
		case EAssetType::Mesh:
			return "Mesh";
		case EAssetType::Shader:
			return "Shader";
		case EAssetType::Material:
			return "Material";
		case EAssetType::Font:
			return "Font";
		case EAssetType::SpriteAtlas:
			return "SpriteAtlas";
		case EAssetType::Unknown:
		default:
			return "Unknown";
		}
	}

	FString WidePathToUtf8(const FWString& Path)
	{
		const fs::path FilePath(Path);
		const std::u8string Utf8Path = FilePath.u8string();
		return FString(reinterpret_cast<const char*>(Utf8Path.data()), Utf8Path.size());
	}

	uint64 ToTicks(const fs::file_time_type& Time)
	{
		return static_cast<uint64>(Time.time_since_epoch().count());
	}

	uint64 ComputeFnv1a64(const uint8* Data, size_t Size)
	{
		uint64 Hash = 14695981039346656037ull;
		for (size_t i = 0; i < Size; ++i)
		{
			Hash ^= static_cast<uint64>(Data[i]);
			Hash *= 1099511628211ull;
		}
		return Hash;
	}

	FString ToHexString(uint64 Value)
	{
		std::ostringstream Stream;
		Stream << std::hex << std::setw(16) << std::setfill('0') << Value;
		return Stream.str();
	}

	FWString NormalizeAssetPath(const FWString& Path)
	{
		if (Path.empty())
		{
			return {};
		}

		fs::path FilePath(Path);
		std::error_code ErrorCode;

		fs::path Normalized = fs::weakly_canonical(FilePath, ErrorCode);
		if (ErrorCode)
		{
			ErrorCode.clear();
			Normalized = FilePath.lexically_normal();
		}
		else
		{
			Normalized = Normalized.lexically_normal();
		}

		FWString Result = Normalized.native();

		std::ranges::replace(Result, L'/', L'\\');
		std::ranges::transform(Result, Result.begin(),
		                       [](wchar_t Ch)
		                       {
			                       return static_cast<wchar_t>(std::towlower(Ch));
		                       });

		return Result;
	}
}

const FSourceRecord* FSourceCache::GetOrLoad(const FWString& Path)
{
	namespace fs = std::filesystem;
	// 1. ??낆젾 野껋럥以??類?뇣??
	FWString NormalizedPath = NormalizePath(Path);
	if (NormalizedPath.empty())
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] Failed at path normalization. Input path is empty or invalid.");
		return nullptr;
	}

	// 2. ???뵬 鈺곕똻???類ㅼ뵥
	fs::path FilePath(NormalizedPath);
	std::error_code ErrorCode;

	if (!fs::exists(FilePath, ErrorCode) || !fs::is_regular_file(FilePath, ErrorCode))
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] Failed at source existence check. Path=%s",
		       WidePathToUtf8(NormalizedPath).c_str());
		return nullptr;
	}

	// 3. ?袁⑹삺 file size / last write time 鈺곌퀬??
	const uint64 CurrentFileSize = fs::file_size(FilePath, ErrorCode);
	if (ErrorCode)
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] Failed at file_size query. Path=%s",
		       WidePathToUtf8(NormalizedPath).c_str());
		return nullptr;
	}

	const auto CurrentWriteTime = fs::last_write_time(FilePath, ErrorCode);
	if (ErrorCode)
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] Failed at last_write_time query. Path=%s",
		       WidePathToUtf8(NormalizedPath).c_str());
		return nullptr;
	}

	// 4. 筌?Ŋ???疫꿸퀣??record揶쎛 ??됱몵筌?癰궰野???? ?類ㅼ뵥
	const uint64 CurrentWriteTimeTicks = ToTicks(CurrentWriteTime);

	auto It = Records.find(NormalizedPath);
	if (It != Records.end() && !HasFileChanged(It->second, CurrentFileSize, CurrentWriteTimeTicks))
	{
		// 5. ??獄쏅뗀??????겹늺 域밸챶?嚥?獄쏆꼹??
		return &It->second;
	}

	// 6. 獄쏅뗀????椰꾧퀡援???곸몵筌????뵬 ??쇰뻻 ??꾨┛
	FSourceRecord NewRecord;
	if (!BuildSourceRecord(NormalizedPath, NewRecord))
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] Failed at source record build. Path=%s",
		       WidePathToUtf8(NormalizedPath).c_str());
		return nullptr;
	}

	// 7. hash ?④쑴沅???record 揶쏄퉮??
	auto [InsertedIt, _] = Records.insert_or_assign(NormalizedPath, std::move(NewRecord));

	// 8. record 獄쏆꼹??
	return &InsertedIt->second;
}

void FSourceCache::Invalidate(const FWString& Path)
{
	const FWString NormalizedPath = NormalizePath(Path);
	if (!NormalizedPath.empty())
	{
		Records.erase(NormalizedPath);
	}
}

void FSourceCache::Clear()
{
	Records.clear();
}

bool FSourceCache::BuildSourceRecord(const FWString& NormalizedPath, FSourceRecord& OutRecord)
{
	namespace fs = std::filesystem;

	const fs::path FilePath(NormalizedPath);
	std::error_code ErrorCode;

	if (!fs::exists(FilePath, ErrorCode) || !fs::is_regular_file(FilePath, ErrorCode))
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] BuildSourceRecord failed at source existence check. Path=%s",
		       WidePathToUtf8(NormalizedPath).c_str());
		return false;
	}

	const uint64 FileSize = fs::file_size(FilePath, ErrorCode);
	if (ErrorCode)
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] BuildSourceRecord failed at file_size query. Path=%s",
		       WidePathToUtf8(NormalizedPath).c_str());
		return false;
	}

	const auto LastWriteTime = fs::last_write_time(FilePath, ErrorCode);
	if (ErrorCode)
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] BuildSourceRecord failed at last_write_time query. Path=%s",
		       WidePathToUtf8(NormalizedPath).c_str());
		return false;
	}

	std::ifstream File(FilePath, std::ios::binary);
	if (!File.is_open())
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetSource] BuildSourceRecord failed at file open. Path=%s",
		       WidePathToUtf8(NormalizedPath).c_str());
		return false;
	}

	OutRecord = {};
	OutRecord.NormalizedPath = NormalizedPath;
	OutRecord.FileSize = FileSize;
	OutRecord.LastWriteTimeTicks = ToTicks(LastWriteTime);
	OutRecord.FileBytes.resize(FileSize);

	if (FileSize > 0)
	{
		File.read(reinterpret_cast<char*>(OutRecord.FileBytes.data()), static_cast<std::streamsize>(FileSize));
		if (!File)
		{
			UE_LOG(Asset, ELogVerbosity::Warning,
			       "[AssetSource] BuildSourceRecord failed at file read. Path=%s",
			       WidePathToUtf8(NormalizedPath).c_str());
			return false;
		}
	}

	const uint64 HashValue = ComputeFnv1a64(OutRecord.FileBytes.data(), OutRecord.FileBytes.size());
	OutRecord.SourceHash = ToHexString(HashValue);
	OutRecord.bFileBytesLoaded = true;

	return true;
}

bool FSourceCache::HasFileChanged(const FSourceRecord& Record, uint64 CurrentFileSize,
	uint64 CurrentWriteTimeTicks) const
{
	return !Record.bFileBytesLoaded
		|| Record.FileSize != CurrentFileSize
		|| Record.LastWriteTimeTicks != CurrentWriteTimeTicks;
}

FWString FSourceCache::NormalizePath(const FWString& Path) const
{
	return NormalizeAssetPath(Path);
}

void UAssetManager::RegisterLoader(IAssetLoader* Loader)
{
	if (!Loader)
	{
		return;
	}

	auto It = std::ranges::find(Loaders, Loader);
	if (It == Loaders.end())
	{
		Loaders.push_back(Loader);
	}
}

UAssetManager::~UAssetManager()
{
	Clear();
}

UAsset* UAssetManager::Load(const FWString& Path, const FAssetLoadParams& Params)
{
	IAssetLoader* Loader = FindLoader(Path, Params);
	if (!Loader)
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetLoad] Failed at loader selection. Path=%s ExplicitType=%s",
		       WidePathToUtf8(Path).c_str(),
		       AssetTypeToString(Params.ExplicitType));
		return nullptr;
	}

	const FSourceRecord* Source = SourceCache.GetOrLoad(Path);
	if (!Source)
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetLoad] Failed at source load. Path=%s LoaderType=%s",
		       WidePathToUtf8(Path).c_str(),
		       AssetTypeToString(Loader->GetAssetType()));
		return nullptr;
	}

	const FAssetKey Key = MakeAssetKey(*Source, *Loader, Params);

	if (!Params.bForceReload)
	{
		auto It = LoadedAssets.find(Key);
		if (It != LoadedAssets.end())
		{
			UAsset* ExistingAsset = It->second.get();
			if (ExistingAsset && ExistingAsset->GetHash() == Source->SourceHash)
			{
				return ExistingAsset;
			}
		}
	}

	UAsset* NewAsset = Loader->LoadAsset(*Source, Params);
	if (!NewAsset)
	{
		UE_LOG(Asset, ELogVerbosity::Warning,
		       "[AssetLoad] Failed at loader execution. Path=%s LoaderType=%s SourceHash=%s",
		       WidePathToUtf8(Source->NormalizedPath).c_str(),
		       AssetTypeToString(Loader->GetAssetType()),
		       Source->SourceHash.c_str());
		return nullptr;
	}

	LoadedAssets.insert_or_assign(Key, std::unique_ptr<UAsset>(NewAsset));
	return NewAsset;
}

void UAssetManager::Invalidate(const FWString& Path)
{
	const FWString NormalizedPath = NormalizeAssetPath(Path);
	if (NormalizedPath.empty())
	{
		return;
	}

	SourceCache.Invalidate(NormalizedPath);

	for (auto It = LoadedAssets.begin(); It != LoadedAssets.end();)
	{
		if (It->first.NormalizedPath == NormalizedPath)
		{
			It = LoadedAssets.erase(It);
		}
		else
		{
			++It;
		}
	}
}

void UAssetManager::Clear()
{
	SourceCache.Clear();
	LoadedAssets.clear();
}

IAssetLoader* UAssetManager::FindLoader(const FWString& Path, const FAssetLoadParams& Params) const
{
	for (IAssetLoader* Loader : Loaders)
	{
		if (!Loader)
		{
			continue;
		}

		if (Params.ExplicitType != EAssetType::Unknown && Loader->GetAssetType() != Params.ExplicitType)
		{
			continue;
		}

		if (Loader->CanLoad(Path, Params))
		{
			return Loader;
		}
	}

	return nullptr;
}

FAssetKey UAssetManager::MakeAssetKey(const FSourceRecord& Source, const IAssetLoader& Loader,
	const FAssetLoadParams& Params) const
{
	FAssetKey Key;
	Key.Type = Loader.GetAssetType();
	Key.NormalizedPath = Source.NormalizedPath;
	Key.BuildSignature = Loader.MakeBuildSignature(Params);
	return Key;
}

