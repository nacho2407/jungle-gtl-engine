#pragma once

#include "Core/CoreTypes.h"
#include "Serialization/Archive.h"

enum class EAssetPackageType : uint32
{
	Unknown = 0,
	StaticMesh,
	SkeletalMesh,
};

struct FAssetPackageHeader
{
	static constexpr uint32 MagicValue = 0x54455341; // ASET
	static constexpr uint32 CurrentVersion = 2;

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
