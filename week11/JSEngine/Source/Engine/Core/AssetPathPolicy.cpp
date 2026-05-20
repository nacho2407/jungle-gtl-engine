#include "Core/AssetPathPolicy.h"

#include "Core/Paths.h"

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>

namespace
{
	bool EndsWith(const FString& Value, const FString& Suffix)
	{
		return Value.size() >= Suffix.size() &&
			Value.compare(Value.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
	}

	FString SanitizeFileNameToken(FString Token)
	{
		if (Token.empty())
		{
			return "Default";
		}

		for (char& Ch : Token)
		{
			const bool bValid =
				(Ch >= 'a' && Ch <= 'z') ||
				(Ch >= 'A' && Ch <= 'Z') ||
				(Ch >= '0' && Ch <= '9') ||
				Ch == '_' ||
				Ch == '-';

			if (!bValid)
			{
				Ch = '_';
			}
		}

		return Token;
	}

	FString ToLowerNormalizedPath(const FString& Path)
	{
		FString Result = FPaths::Normalize(Path);
		std::replace(Result.begin(), Result.end(), '\\', '/');
		std::transform(
			Result.begin(),
			Result.end(),
			Result.begin(),
			[](unsigned char Ch)
			{
				return static_cast<char>(std::tolower(Ch));
			});
		return Result;
	}

	bool HasAssetDirectory(const FString& Path, const FString& Directory)
	{
		const FString LowerPath = ToLowerNormalizedPath(Path);
		const FString LowerDirectory = ToLowerNormalizedPath(Directory);
		return LowerPath.rfind(LowerDirectory + "/", 0) == 0 ||
			LowerPath.find("/" + LowerDirectory + "/") != FString::npos;
	}

	std::wstring GetLowerExtension(const FString& Path)
	{
		std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
		std::wstring Extension = FsPath.extension().wstring();
		std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
		return Extension;
	}
}

bool FAssetPathPolicy::FileExists(const FString& Path)
{
	std::error_code Ec;
	return std::filesystem::exists(std::filesystem::path(FPaths::ToAbsolute(FPaths::ToWide(Path))), Ec) && !Ec;
}

bool FAssetPathPolicy::IsCurveAssetPath(const FString& Path)
{
	std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
	FString FileName = FPaths::ToUtf8(FsPath.filename().wstring());
	std::transform(
		FileName.begin(),
		FileName.end(),
		FileName.begin(),
		[](unsigned char Ch)
		{
			return static_cast<char>(std::tolower(Ch));
		});

	return EndsWith(FileName, ".curve");
}

bool FAssetPathPolicy::IsAnimSequenceAssetPath(const FString& Path)
{
	std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
	std::wstring Extension = FsPath.extension().wstring();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
	return Extension == L".animsequence";
}

bool FAssetPathPolicy::IsSequenceAssetPath(const FString& Path)
{
	std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
	std::wstring Extension = FsPath.extension().wstring();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
	return Extension == L".sequence";
}

bool FAssetPathPolicy::IsSerializedMaterialAssetPath(const FString& Path)
{
	std::filesystem::path FsPath(FPaths::ToWide(FPaths::Normalize(Path)));
	std::wstring Extension = FsPath.extension().wstring();
	std::transform(Extension.begin(), Extension.end(), Extension.begin(), ::towlower);
	return Extension == L".mat" || Extension == L".matinst";
}

bool FAssetPathPolicy::IsSkeletalMeshSourcePath(const FString& Path)
{
	return GetLowerExtension(Path) == L".fbx" && HasAssetDirectory(Path, "Asset/SkeletalMesh");
}

bool FAssetPathPolicy::IsStaticMeshSourcePath(const FString& Path)
{
	const std::wstring Extension = GetLowerExtension(Path);
	if (Extension == L".obj")
	{
		return true;
	}

	return Extension == L".fbx" && !IsSkeletalMeshSourcePath(Path);
}

FString FAssetPathPolicy::MakeCookedStaticMeshBinaryPath(const FString& SourcePath)
{
	std::filesystem::path SourceFsPath(FPaths::ToWide(FPaths::Normalize(SourcePath)));
	const std::filesystem::path AssetMeshRoot = std::filesystem::path(L"Asset") / L"Mesh";
	std::filesystem::path SubPath = SourceFsPath.lexically_normal().lexically_relative(AssetMeshRoot);
	if (SubPath.empty())
	{
		SubPath = SourceFsPath.filename();
	}
	else
	{
		for (const std::filesystem::path& Part : SubPath)
		{
			if (Part == L"..")
			{
				SubPath = SourceFsPath.filename();
				break;
			}
		}
	}

	SubPath.replace_extension(L".bin");
	return FPaths::ToUtf8((std::filesystem::path(L"Asset") / L"Cooked" / L"Mesh" / SubPath).generic_wstring());
}

FString FAssetPathPolicy::MakeSiblingStaticMeshBinaryPath(const FString& SourcePath)
{
	std::filesystem::path SourceFsPath(FPaths::ToWide(FPaths::Normalize(SourcePath)));
	SourceFsPath.replace_extension(L".bin");
	return FPaths::ToUtf8(SourceFsPath.generic_wstring());
}

FString FAssetPathPolicy::MakeStaticMeshCacheBinaryPath(const FString& SourcePath)
{
	std::filesystem::path SourceFsPath(FPaths::ToWide(FPaths::Normalize(SourcePath)));
	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += L".bin";
	return FPaths::ToUtf8((std::filesystem::path(L"Asset") / L"Mesh" / L"Bin" / BinaryFileName).generic_wstring());
}

FString FAssetPathPolicy::MakeWritableStaticMeshCacheBinaryPath(const FString& SourcePath)
{
	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));

	std::filesystem::path BinDir = std::filesystem::path(FPaths::RootDir()) / "Asset" / "Mesh" / "Bin";

	if (!std::filesystem::exists(BinDir))
	{
		std::filesystem::create_directories(BinDir);
	}

	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += ".bin";

	std::filesystem::path BinaryPath = BinDir / BinaryFileName;
	return FPaths::ToString(BinaryPath.wstring());
}

FString FAssetPathPolicy::MakeWritableSkeletalMeshCacheBinaryPath(const FString& SourcePath)
{
	// StaticMesh와 stem 이 겹칠 수 있어 SkeletalMesh 전용 루트로 분리.
	const FString NormalizedSourcePath = FPaths::Normalize(SourcePath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));

	std::filesystem::path BinDir = std::filesystem::path(FPaths::RootDir()) / "Asset" / "SkeletalMesh" / "Bin";

	if (!std::filesystem::exists(BinDir))
	{
		std::filesystem::create_directories(BinDir);
	}

	std::filesystem::path BinaryFileName = SourceFsPath.stem();
	BinaryFileName += ".bin";

	std::filesystem::path BinaryPath = BinDir / BinaryFileName;
	return FPaths::ToString(BinaryPath.wstring());
}

FString FAssetPathPolicy::MakeWritableAnimSequenceCacheBinaryPath(const FString& SourceFbxPath, const FString& TargetSkeletalMeshPath, const FString& AnimStackName)
{
	const FString NormalizedSourcePath = FPaths::Normalize(SourceFbxPath);
	std::filesystem::path SourceFsPath(FPaths::ToWide(NormalizedSourcePath));
	const FString NormalizedTargetPath = FPaths::Normalize(TargetSkeletalMeshPath);
	std::filesystem::path TargetFsPath(FPaths::ToWide(NormalizedTargetPath));

	std::filesystem::path BinDir = std::filesystem::path(FPaths::RootDir()) / "Asset" / "AnimSequence" / "Bin";

	if (!std::filesystem::exists(BinDir))
	{
		std::filesystem::create_directories(BinDir);
	}

	const std::wstring BinaryFileName =
		SourceFsPath.stem().wstring() +
		L"_" +
		FPaths::ToWide(SanitizeFileNameToken(AnimStackName)) +
		L"_" +
		FPaths::ToWide(SanitizeFileNameToken(FPaths::ToString(TargetFsPath.stem().wstring()))) +
		L".anim.bin";

	std::filesystem::path BinaryPath = BinDir / BinaryFileName;
	return FPaths::ToString(BinaryPath.wstring());
}
