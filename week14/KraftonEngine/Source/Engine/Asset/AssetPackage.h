#pragma once

#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"

enum class EAssetPackageType : uint32
{
	Unknown = 0,
	StaticMesh,
	SkeletalMesh,
	FloatCurve,
	CameraShake,
	Material,
};

struct FAssetPackageHeader
{
	static constexpr uint32 MagicValue = 0x54455341; // ASET
	static constexpr uint32 CurrentVersion = 3;

	uint32 Magic = MagicValue;
	uint32 Version = CurrentVersion;
	uint32 Type = static_cast<uint32>(EAssetPackageType::Unknown);

	friend FArchive& operator<<(FArchive& Ar, FAssetPackageHeader& Header)
	{
		Ar << Header.Magic;
		Ar << Header.Version;
		Ar << Header.Type;
		return Ar;
	}

	bool IsValid(EAssetPackageType ExpectedType) const
	{
		return Magic == MagicValue
			&& Version == CurrentVersion
			&& Type == static_cast<uint32>(ExpectedType);
	}

	bool IsValidPackage() const
	{
		return Magic == MagicValue
			&& Version == CurrentVersion;
	}
};

struct FAssetImportMetadata
{
	FString SourcePath;
	uint64 SourceTimestamp = 0;
	uint64 SourceFileSize = 0;

	friend FArchive& operator<<(FArchive& Ar, FAssetImportMetadata& Metadata)
	{
		Ar << Metadata.SourcePath;
		Ar << Metadata.SourceTimestamp;
		Ar << Metadata.SourceFileSize;
		return Ar;
	}

	bool IsSourceAvailable() const
	{
		return !SourcePath.empty();
	}

	bool MatchesSource(uint64 CurrentTimestamp, uint64 CurrentFileSize) const
	{
		return SourceTimestamp == CurrentTimestamp
			&& SourceFileSize == CurrentFileSize;
	}
};

class FAssetPackage
{
public:
	static bool IsAssetPackagePath(const FString& Path);
	static bool ReadHeader(const FString& Path, FAssetPackageHeader& OutHeader);
	static bool GetPackageType(const FString& Path, EAssetPackageType& OutType);
	static bool ReadMetadata(const FString& Path, EAssetPackageType ExpectedType, FAssetImportMetadata& OutMetadata);

	static bool SaveStringPayload(const FString& Path, EAssetPackageType Type, const FAssetImportMetadata& Metadata, const FString& Payload);
	static bool LoadStringPayload(const FString& Path, EAssetPackageType ExpectedType, FAssetImportMetadata& Metadata, FString& OutPayload);
};
