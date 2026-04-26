#pragma once
#include "Render/Types/RenderTypes.h"

struct FMaterialParameterInfo;

// 셰이더 컴파일 실패 시 에러 처리 방식
enum class EShaderErrorMode
{
	Notification,  // 핫 리로드: 에디터 알림만 (기본)
	MessageBox,    // 첫 시작: MessageBox로 즉시 알림
};

// ============================================================
// FComputeShader — ID3D11ComputeShader* 래퍼 (FShaderManager가 소유)
// ============================================================
class FComputeShader
{
public:
	FComputeShader() = default;
	~FComputeShader() { Release(); }

	FComputeShader(const FComputeShader&) = delete;
	FComputeShader& operator=(const FComputeShader&) = delete;

	bool Create(ID3D11Device* InDevice, const wchar_t* Path, const char* EntryPoint,
		TArray<FString>* OutIncludes = nullptr);
	void Release();

	void Bind(ID3D11DeviceContext* Ctx) const
	{
		Ctx->CSSetShader(CS, nullptr, 0);
	}

	bool IsValid() const { return CS != nullptr; }
	ID3D11ComputeShader* Get() const { return CS; }

	// 핫 리로드: 내부 COM 포인터 교체 (기존 포인터 Release 후 새 포인터 대입)
	void Swap(ID3D11ComputeShader* NewCS)
	{
		if (CS) CS->Release();
		CS = NewCS;
	}

	// 소유권 분리: 내부 포인터를 반환하고 nullptr로 설정 (Release 안 함)
	ID3D11ComputeShader* Detach()
	{
		ID3D11ComputeShader* Tmp = CS;
		CS = nullptr;
		return Tmp;
	}

private:
	ID3D11ComputeShader* CS = nullptr;
};

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
		const D3D_SHADER_MACRO* InDefines = nullptr, TArray<FString>* OutIncludes = nullptr,
		EShaderErrorMode ErrorMode = EShaderErrorMode::Notification);
	void Release();

	void Bind(ID3D11DeviceContext* InDeviceContext) const;

	bool IsValid() const { return VertexShader != nullptr && PixelShader != nullptr; }

	const TMap<FString, FMaterialParameterInfo*>& GetParameterLayout() const { return ShaderParameterLayout; }
private:
	ID3D11VertexShader* VertexShader = nullptr;
	ID3D11PixelShader* PixelShader = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;

	size_t CachedVertexShaderSize = 0;
	size_t CachedPixelShaderSize = 0;

	void CreateInputLayoutFromReflection(ID3D11Device* InDevice, ID3DBlob* VSBlob);
	void ExtractCBufferInfo(ID3DBlob* ShaderBlob, TMap<FString, FMaterialParameterInfo*>& OutLayout);
	TMap<FString, FMaterialParameterInfo*> ShaderParameterLayout;
};
