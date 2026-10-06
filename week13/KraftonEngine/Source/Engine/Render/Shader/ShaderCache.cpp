#include "ShaderCache.h"

#include "Platform/Paths.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace ShaderCache
{
	uint64 ShaderCacheFNV1a(const void* Data, size_t Size, uint64 Seed = 14695981039346656037ULL)
	{
		const uint8* Bytes = static_cast<const uint8*>(Data);
		uint64 Hash = Seed;
		for (size_t Index = 0; Index < Size; ++Index)
		{
			Hash ^= Bytes[Index];
			Hash *= 1099511628211ULL;
		}
		return Hash;
	}

	std::string NormalizeDefines(const D3D_SHADER_MACRO* Defines)
	{
		if (!Defines)
		{
			return {};
		}

		std::vector<std::string> Pairs;
		for (const D3D_SHADER_MACRO* Define = Defines; Define->Name != nullptr; ++Define)
		{
			std::string Pair = Define->Name;
			Pair += '=';
			if (Define->Definition)
			{
				Pair += Define->Definition;
			}
			Pairs.push_back(std::move(Pair));
		}

		std::sort(Pairs.begin(), Pairs.end());

		std::ostringstream Stream;
		for (const std::string& Pair : Pairs)
		{
			Stream << Pair << ';';
		}
		return Stream.str();
	}

	uint64 ShaderCacheHashFile(const std::filesystem::path& FilePath, uint64 Seed)
	{
		std::ifstream File(FilePath, std::ios::binary);
		if (!File.is_open())
		{
			return Seed;
		}

		std::vector<char> Buffer((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
		if (Buffer.empty())
		{
			return Seed;
		}

		return ShaderCacheFNV1a(Buffer.data(), Buffer.size(), Seed);
	}

	uint64 ShaderCacheHashAllIncludes(uint64 Seed)
	{
		namespace fs = std::filesystem;

		const fs::path ShaderDir(FPaths::ShaderDir());
		std::error_code Error;
		if (!fs::exists(ShaderDir, Error))
		{
			return Seed;
		}

		std::vector<fs::path> Includes;
		for (fs::recursive_directory_iterator It(ShaderDir, Error), End; !Error && It != End; It.increment(Error))
		{
			if (It->is_regular_file(Error) && It->path().extension() == L".hlsli")
			{
				Includes.push_back(It->path());
			}
		}

		std::sort(Includes.begin(), Includes.end());

		uint64 Hash = Seed;
		for (const fs::path& IncludePath : Includes)
		{
			const std::string Name = IncludePath.filename().string();
			Hash = ShaderCacheFNV1a(Name.data(), Name.size(), Hash);
			Hash = ShaderCacheHashFile(IncludePath, Hash);
		}
		return Hash;
	}

	std::wstring BuildShaderCachePath(const std::string& CacheKey)
	{
		return FPaths::ShaderCacheDir() + FPaths::ToWide(CacheKey) + L".cso";
	}

	std::string BuildShaderCacheKey(const wchar_t* FilePath, const char* EntryPoint, const char* Target, const D3D_SHADER_MACRO* Defines)
	{
		uint64 Hash = 14695981039346656037ULL;

		if (FilePath && FilePath[0] != L'\0')
		{
			Hash = ShaderCacheHashFile(FilePath, Hash);
		}

		Hash = ShaderCacheHashAllIncludes(Hash);

		const std::string DefineString = NormalizeDefines(Defines);
		if (!DefineString.empty())
		{
			Hash = ShaderCacheFNV1a(DefineString.data(), DefineString.size(), Hash);
		}

		if (EntryPoint)
		{
			Hash = ShaderCacheFNV1a(EntryPoint, std::strlen(EntryPoint), Hash);
		}

		Hash = ShaderCacheFNV1a(":", 1, Hash);

		if (Target)
		{
			Hash = ShaderCacheFNV1a(Target, std::strlen(Target), Hash);
		}

		char Buffer[24];
		std::snprintf(Buffer, sizeof(Buffer), "%016llx", static_cast<unsigned long long>(Hash));
		return std::string(Buffer);
	}

	bool LoadShaderBlob(const std::string& CacheKey, ID3DBlob** OutBlob)
	{
		if (!OutBlob)
		{
			return false;
		}

		const std::wstring CachePath = BuildShaderCachePath(CacheKey);
		std::ifstream File(CachePath, std::ios::binary | std::ios::ate);
		if (!File.is_open())
		{
			return false;
		}

		const std::streamsize Size = File.tellg();
		if (Size <= 0)
		{
			return false;
		}
		File.seekg(0);

		ID3DBlob* Blob = nullptr;
		if (FAILED(D3DCreateBlob(static_cast<SIZE_T>(Size), &Blob)) || !Blob)
		{
			return false;
		}

		File.read(static_cast<char*>(Blob->GetBufferPointer()), Size);
		if (!File)
		{
			Blob->Release();
			return false;
		}

		*OutBlob = Blob;
		return true;
	}

	void SaveShaderBlob(const std::string& CacheKey, ID3DBlob* Blob)
	{
		if (!Blob)
		{
			return;
		}

		std::error_code Error;
		std::filesystem::create_directories(FPaths::ShaderCacheDir(), Error);

		const std::wstring CachePath = BuildShaderCachePath(CacheKey);
		std::ofstream File(CachePath, std::ios::binary | std::ios::trunc);
		if (!File.is_open())
		{
			return;
		}

		File.write(static_cast<const char*>(Blob->GetBufferPointer()), static_cast<std::streamsize>(Blob->GetBufferSize()));
	}
}
