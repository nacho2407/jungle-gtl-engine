#include "AssetPackage.h"

#include "Platform/Paths.h"
#include "Serialization/WindowsArchive.h"

#include <algorithm>
#include <filesystem>

static std::wstring GetLowerExtension(const FString& Path)
{
	std::filesystem::path SrcPath(FPaths::ToWide(Path));
	std::wstring Ext = SrcPath.extension().wstring();
	std::transform(Ext.begin(), Ext.end(), Ext.begin(), ::towlower);
	return Ext;
}

bool FAssetPackage::IsAssetPackagePath(const FString& Path)
{
	std::wstring Ext = GetLowerExtension(Path);
	return Ext == L".uasset";
}

bool FAssetPackage::ReadHeader(const FString& Path, FAssetPackageHeader& OutHeader)
{
	FWindowsBinReader Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid()) return false;

	Ar << OutHeader;
	return Ar.IsValid() && OutHeader.IsValidPackage();
}

bool FAssetPackage::GetPackageType(const FString& Path, EAssetPackageType& OutType)
{
	FAssetPackageHeader Header;
	if (!ReadHeader(Path, Header))
	{
		OutType = EAssetPackageType::Unknown;
		return false;
	}

	OutType = static_cast<EAssetPackageType>(Header.Type);
	return true;
}

bool FAssetPackage::ReadMetadata(const FString& Path, EAssetPackageType ExpectedType, FAssetImportMetadata& OutMetadata)
{
	FWindowsBinReader Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid()) return false;

	FAssetPackageHeader Header;
	Ar << Header;

	if (!Header.IsValid(ExpectedType))
	{
		return false;
	}
	
	Ar << OutMetadata;
	return Ar.IsValid();
}

bool FAssetPackage::SaveStringPayload(const FString& Path, EAssetPackageType Type, const FAssetImportMetadata& Metadata, const FString& Payload)
{
	// 패키지 헤더 + 임포트 메타데이터 + 문자열 페이로드를 순서대로 저장합니다.
	FWindowsBinWriter Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid()) return false;

	FAssetPackageHeader Header;
	Header.Type = static_cast<uint32>(Type);

	FAssetImportMetadata MetadataCopy = Metadata;
	FString PayloadCopy = Payload;

	Ar << Header;
	Ar << MetadataCopy;
	Ar << PayloadCopy;

	return Ar.IsValid();
}

bool FAssetPackage::LoadStringPayload(const FString& Path, EAssetPackageType ExpectedType, FAssetImportMetadata& Metadata, FString& OutPayload)
{
	FWindowsBinReader Ar(FPaths::MakeProjectRelative(Path));
	if (!Ar.IsValid()) return false;

	FAssetPackageHeader Header;
	Ar << Header;

	if (!Header.IsValid(ExpectedType))
	{
		return false;
	}

	Ar << Metadata;
	Ar << OutPayload;

	return Ar.IsValid();
}
