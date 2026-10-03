#include "ShaderCompiler.h"
#include "Core/Paths.h"

#include <d3dcompiler.h>
#include <filesystem>
#include <fstream>
#include <vector>

static FString GetShaderDefineFingerprint(const D3D_SHADER_MACRO* Defines)
{
	FString Result;
	for (const D3D_SHADER_MACRO* Define = Defines; Define && Define->Name; ++Define)
	{
		Result += Define->Name;
		Result += "=";
		Result += Define->Definition ? Define->Definition : "";
		Result += ";";
	}
	return Result;
}

static uint64 HashShaderCacheKey(const FString& Key)
{
	uint64 Hash = 1469598103934665603ull;
	for (char Ch : Key)
	{
		Hash ^= static_cast<unsigned char>(Ch);
		Hash *= 1099511628211ull;
	}
	return Hash;
}

static FString ToHexString(uint64 Value)
{
	constexpr char HexDigits[] = "0123456789abcdef";
	FString Result(16, '0');
	for (int32 Index = 15; Index >= 0; --Index)
	{
		Result[Index] = HexDigits[Value & 0xF];
		Value >>= 4;
	}
	return Result;
}

static bool TryExtractIncludePath(const FString& Line, FString& OutInclude)
{
	const size_t IncludePos = Line.find("#include");
	if (IncludePos == FString::npos)
	{
		return false;
	}

	const size_t FirstQuote = Line.find('"', IncludePos);
	if (FirstQuote == FString::npos)
	{
		return false;
	}

	const size_t SecondQuote = Line.find('"', FirstQuote + 1);
	if (SecondQuote == FString::npos || SecondQuote <= FirstQuote + 1)
	{
		return false;
	}

	OutInclude = Line.substr(FirstQuote + 1, SecondQuote - FirstQuote - 1);
	return true;
}

static void CollectShaderDependenciesRecursive(
	const std::filesystem::path& SourcePath,
	std::vector<std::filesystem::path>& OutDependencies,
	std::vector<std::filesystem::path>& VisitedFiles)
{
	const std::filesystem::path NormalizedPath = SourcePath.lexically_normal();
	for (const std::filesystem::path& VisitedFile : VisitedFiles)
	{
		if (VisitedFile == NormalizedPath)
		{
			return;
		}
	}

	VisitedFiles.push_back(NormalizedPath);
	OutDependencies.push_back(NormalizedPath);

	std::ifstream In(NormalizedPath);
	if (!In.is_open())
	{
		return;
	}

	FString Line;
	while (std::getline(In, Line))
	{
		FString IncludePath;
		if (!TryExtractIncludePath(Line, IncludePath))
		{
			continue;
		}

		std::filesystem::path IncludeFsPath(FPaths::ToWide(IncludePath));
		if (IncludeFsPath.is_relative())
		{
			IncludeFsPath = NormalizedPath.parent_path() / IncludeFsPath;
		}

		CollectShaderDependenciesRecursive(IncludeFsPath.lexically_normal(), OutDependencies, VisitedFiles);
	}
}

static bool IsShaderCacheUpToDate(const std::filesystem::path& CachePath, const std::filesystem::path& SourcePath)
{
	std::error_code Ec;
	if (!std::filesystem::exists(CachePath, Ec) || Ec)
	{
		return false;
	}

	const std::filesystem::file_time_type CacheTime = std::filesystem::last_write_time(CachePath, Ec);
	if (Ec)
	{
		return false;
	}

	std::vector<std::filesystem::path> Dependencies;
	std::vector<std::filesystem::path> VisitedFiles;
	CollectShaderDependenciesRecursive(SourcePath, Dependencies, VisitedFiles);

	for (const std::filesystem::path& Dependency : Dependencies)
	{
		const std::filesystem::file_time_type DependencyTime = std::filesystem::last_write_time(Dependency, Ec);
		if (Ec || DependencyTime > CacheTime)
		{
			return false;
		}
	}

	return true;
}

static std::filesystem::path MakeShaderCachePath(
	const FString& FilePath,
	const FString& EntryPoint,
	const FString& Target,
	const D3D_SHADER_MACRO* Defines,
	uint32 PermutationKey)
{
	const FString NormalizedPath = FPaths::Normalize(FilePath);
	const FString Fingerprint = NormalizedPath
		+ "|" + EntryPoint
		+ "|" + Target
		+ "|" + std::to_string(PermutationKey)
		+ "|" + GetShaderDefineFingerprint(Defines)
		+ "|Flags=0";

	std::filesystem::path CacheDir = std::filesystem::path(FPaths::RootDir()) / L"DerivedData" / L"ShaderCache" / FPaths::ToWide(Target);
	std::filesystem::create_directories(CacheDir);

	const FString CacheFileName = ToHexString(HashShaderCacheKey(Fingerprint)) + ".cso";
	return CacheDir / FPaths::ToWide(CacheFileName);
}

FShaderCompileResult FShaderCompiler::CompileFromFile(const FString& FilePath, const FString& EntryPoint, const FString& Target,
	const D3D_SHADER_MACRO* Defines, uint32 PermutationKey)
{
	FShaderCompileResult Result;
	const FString NormalizedFilePath = FPaths::Normalize(FilePath);
	const std::filesystem::path SourcePath(FPaths::ToAbsolute(FPaths::ToWide(NormalizedFilePath)));
	const std::filesystem::path CachePath = MakeShaderCachePath(NormalizedFilePath, EntryPoint, Target, Defines, PermutationKey);

	if (IsShaderCacheUpToDate(CachePath, SourcePath))
	{
		HRESULT ReadHr = D3DReadFileToBlob(CachePath.c_str(), &Result.Blob);
		if (SUCCEEDED(ReadHr))
		{
			Result.bSuccess = true;
			return Result;
		}
	}

	ID3DBlob* ErrorBlob = nullptr;
	HRESULT hr = D3DCompileFromFile(SourcePath.c_str(), Defines, D3D_COMPILE_STANDARD_FILE_INCLUDE, EntryPoint.c_str(), Target.c_str(), 0, 0, &Result.Blob, &ErrorBlob);

	if (FAILED(hr))
	{
		Result.bSuccess = false;
		if (ErrorBlob)
		{
			Result.ErrorMessage = FString((char*)ErrorBlob->GetBufferPointer());
			ErrorBlob->Release();
		}
		else
		{
			Result.ErrorMessage = "Unknown error during shader compilation: " + NormalizedFilePath;
		}
	}
	else
	{
		Result.bSuccess = true;
		D3DWriteBlobToFile(Result.Blob, CachePath.c_str(), TRUE);
	}

	return Result;
}
