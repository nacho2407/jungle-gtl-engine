#pragma once
#include "Render/Types/RenderTypes.h"

struct FMaterialParameterInfo;

class FShader
{
public:
	FShader() = default;
	~FShader() { Release(); }

	FShader(const FShader&) = delete;
	FShader& operator=(const FShader&) = delete;
	FShader(FShader&& Other) noexcept;
	FShader& operator=(FShader&& Other) noexcept;

	void Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
		const D3D11_INPUT_ELEMENT_DESC* InInputElements, uint32 InInputElementCount,
		const D3D_SHADER_MACRO* InDefines = nullptr);
	void Release();

	void Bind(ID3D11DeviceContext* InDeviceContext) const;

	const TMap<FString, FMaterialParameterInfo*>& GetParameterLayout() const { return ShaderParameterLayout; }
private:
	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;

	size_t CachedVertexShaderSize = 0;
	size_t CachedPixelShaderSize = 0;

	void ExtractCBufferInfo(ID3DBlob* ShaderBlob, TMap<FString, FMaterialParameterInfo*>& OutLayout);
	////void ExtractTextureInfo(ID3DBlob* ShaderBlob, TMap<FString, ???>& OutLayout);
	TMap<FString, FMaterialParameterInfo*> ShaderParameterLayout;
};
