#include "ShaderResource.h"
#include "Core/Paths.h"
#include <d3dcompiler.h>
#include <filesystem>
#include <fstream>

#pragma comment(lib, "d3dcompiler.lib")

namespace fs = std::filesystem;

std::unordered_map<std::wstring, std::shared_ptr<FShaderResource>> FShaderResource::Cache;
std::wstring FShaderResource::ContentDir;

FShaderResource::~FShaderResource()
{
	if (ShaderBlob)
	{
		ShaderBlob->Release();
		ShaderBlob = nullptr;
	}
}

const void* FShaderResource::GetBufferPointer() const
{
	if (bFromCso)
	{
		return RawData.data();
	}
	return ShaderBlob ? ShaderBlob->GetBufferPointer() : nullptr;
}

SIZE_T FShaderResource::GetBufferSize() const
{
	if (bFromCso)
	{
		return RawData.size();
	}
	return ShaderBlob ? ShaderBlob->GetBufferSize() : 0;
}

void FShaderResource::SetContentDir(const wchar_t* Dir)
{
	ContentDir = Dir;
	if (!ContentDir.empty() && ContentDir.back() != L'/' && ContentDir.back() != L'\\')
	{
		ContentDir += L'/';
	}
}

std::wstring FShaderResource::MakeCsoPath(const wchar_t* HlslPath, const char* EntryPoint)
{
	fs::path HlslFile(HlslPath);
	std::wstring Stem = HlslFile.stem().wstring();
	std::wstring Entry(EntryPoint, EntryPoint + strlen(EntryPoint));
	return ContentDir + Stem + L"_" + Entry + L".cso";
}

bool FShaderResource::IsHlslNewer(const wchar_t* HlslPath, const wchar_t* CsoPath)
{
	std::error_code Ec;

	if (!fs::exists(CsoPath, Ec))
	{
		return true;
	}

	if (!fs::exists(HlslPath, Ec))
	{
		return false;
	}

	auto HlslTime = fs::last_write_time(HlslPath, Ec);
	if (Ec) return true;

	auto CsoTime = fs::last_write_time(CsoPath, Ec);
	if (Ec) return true;

	return HlslTime > CsoTime;
}

bool FShaderResource::SaveCso(const wchar_t* CsoPath, const void* Data, SIZE_T Size)
{
	fs::path Dir = fs::path(CsoPath).parent_path();
	std::error_code Ec;
	fs::create_directories(Dir, Ec);

	std::ofstream File(CsoPath, std::ios::binary);
	if (!File.is_open())
	{
		return false;
	}

	File.write(static_cast<const char*>(Data), Size);
	return File.good();
}

std::shared_ptr<FShaderResource> FShaderResource::LoadCso(const wchar_t* CsoPath)
{
	std::ifstream File(CsoPath, std::ios::binary | std::ios::ate);
	if (!File.is_open())
	{
		return nullptr;
	}

	std::streamsize Size = File.tellg();
	if (Size <= 0)
	{
		return nullptr;
	}

	File.seekg(0, std::ios::beg);

	std::shared_ptr<FShaderResource> Resource(new FShaderResource());
	Resource->RawData.resize(static_cast<size_t>(Size));
	File.read(reinterpret_cast<char*>(Resource->RawData.data()), Size);

	if (!File.good())
	{
		return nullptr;
	}

	Resource->bFromCso = true;
	return Resource;
}

std::shared_ptr<FShaderResource> FShaderResource::GetOrCompile(
	const wchar_t* FilePath,
	const char* EntryPoint,
	const char* Target)
{
	// ContentDir이 비어 있으면 FPaths에서 초기화
	if (ContentDir.empty())
	{
		std::wstring Temp = FPaths::ShaderCacheDir().wstring();
		SetContentDir(Temp.c_str());
	}

	std::wstring Key = std::wstring(FilePath) + L"|" + std::wstring(EntryPoint, EntryPoint + strlen(EntryPoint));

	auto It = Cache.find(Key);
	if (It != Cache.end())
	{
		return It->second;
	}

	std::wstring CsoPath = MakeCsoPath(FilePath, EntryPoint);

	// .cso가 존재하고 hlsl보다 최신이면 cso에서 로드
	if (!IsHlslNewer(FilePath, CsoPath.c_str()))
	{
		auto Resource = LoadCso(CsoPath.c_str());
		if (Resource)
		{
			Cache[Key] = Resource;
			return Resource;
		}
	}

	// hlsl에서 컴파일
	ID3DBlob* Blob = nullptr;
	ID3DBlob* ErrorBlob = nullptr;

	HRESULT Hr = D3DCompileFromFile(
		FilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		EntryPoint, Target,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &Blob, &ErrorBlob
	);

	if (FAILED(Hr))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(ErrorBlob->GetBufferPointer()));
			ErrorBlob->Release();
		}
		return nullptr;
	}

	if (ErrorBlob)
	{
		ErrorBlob->Release();
	}

	// 컴파일 결과를 .cso로 저장
	SaveCso(CsoPath.c_str(), Blob->GetBufferPointer(), Blob->GetBufferSize());

	std::shared_ptr<FShaderResource> Resource(new FShaderResource());
	Resource->ShaderBlob = Blob;

	Cache[Key] = Resource;
	return Resource;
}

void FShaderResource::ClearCache()
{
	Cache.clear();
}
