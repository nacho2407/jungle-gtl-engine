#pragma once

#include "EngineAPI.h"
#include <d3d11.h>
#include <string>
#include <unordered_map>
#include <memory>

class ENGINE_API FShaderResource
{
public:
	~FShaderResource();

	const void* GetBufferPointer() const;
	SIZE_T GetBufferSize() const;

	static std::shared_ptr<FShaderResource> GetOrCompile(
		const wchar_t* FilePath,
		const char* EntryPoint,
		const char* Target
	);

	static void ClearCache();

	// Content/Shaders 기본 경로 설정
	static void SetContentDir(const wchar_t* Dir);

private:
	FShaderResource() = default;

	// .cso 파일 경로 생성 (Content/Shaders/VertexShader_main.cso)
	static std::wstring MakeCsoPath(const wchar_t* HlslPath, const char* EntryPoint);

	// hlsl이 cso보다 최신인지 확인
	static bool IsHlslNewer(const wchar_t* HlslPath, const wchar_t* CsoPath);

	// .cso 저장/로드
	static bool SaveCso(const wchar_t* CsoPath, const void* Data, SIZE_T Size);
	static std::shared_ptr<FShaderResource> LoadCso(const wchar_t* CsoPath);

	ID3DBlob* ShaderBlob = nullptr;

	// Blob 없이 raw 바이트로 로드한 경우
	std::vector<uint8_t> RawData;
	bool bFromCso = false;

	static std::unordered_map<std::wstring, std::shared_ptr<FShaderResource>> Cache;
	static std::wstring ContentDir;
};
