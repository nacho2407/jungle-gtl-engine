#include "Core/ShaderResourceCache.h"

#include "Core/Paths.h"
#include "Core/Logging/Log.h"
#include "Render/Resource/ShaderCompiler.h"

#include <cstring>

namespace
{
	DXGI_FORMAT GetReflectedDXGIFormat(const D3D11_SIGNATURE_PARAMETER_DESC& ParamDesc)
	{
		if (ParamDesc.Mask == 1)
		{
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) return DXGI_FORMAT_R32_UINT;
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) return DXGI_FORMAT_R32_SINT;
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) return DXGI_FORMAT_R32_FLOAT;
		}
		else if (ParamDesc.Mask <= 3)
		{
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) return DXGI_FORMAT_R32G32_UINT;
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) return DXGI_FORMAT_R32G32_SINT;
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) return DXGI_FORMAT_R32G32_FLOAT;
		}
		else if (ParamDesc.Mask <= 7)
		{
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) return DXGI_FORMAT_R32G32B32_UINT;
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) return DXGI_FORMAT_R32G32B32_SINT;
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) return DXGI_FORMAT_R32G32B32_FLOAT;
		}
		else if (ParamDesc.Mask <= 15)
		{
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) return DXGI_FORMAT_R32G32B32A32_UINT;
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) return DXGI_FORMAT_R32G32B32A32_SINT;
			if (ParamDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) return DXGI_FORMAT_R32G32B32A32_FLOAT;
		}
		return DXGI_FORMAT_UNKNOWN;
	}

	FShaderStageKey NormalizeStageKey(const FShaderStageKey& Key)
	{
		FShaderStageKey Normalized = Key;
		Normalized.FilePath = FPaths::Normalize(Key.FilePath);
		return Normalized;
	}

	uint32 HashVertexLayout(const FVertexLayoutDesc* Layout)
	{
		if (!Layout || Layout->Elements.empty())
		{
			return 0;
		}

		size_t Hash = std::hash<uint32>{}(Layout->Stride);
		for (const FVertexElementDesc& Element : Layout->Elements)
		{
			Hash ^= std::hash<FString>{}(Element.SemanticName) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
			Hash ^= std::hash<uint32>{}(Element.SemanticIndex) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
			Hash ^= std::hash<uint32>{}(static_cast<uint32>(Element.Format)) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
			Hash ^= std::hash<uint32>{}(Element.InputSlot) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
			Hash ^= std::hash<uint32>{}(Element.AlignedByteOffset) + 0x9e3779b9 + (Hash << 6) + (Hash >> 2);
		}

		return static_cast<uint32>(Hash);
	}

	FShaderStageKey NormalizeVertexStageKey(const FShaderStageKey& Key, const FVertexLayoutDesc* VertexLayout)
	{
		FShaderStageKey Normalized = NormalizeStageKey(Key);
		if (Normalized.InputLayoutHash == 0)
		{
			Normalized.InputLayoutHash = HashVertexLayout(VertexLayout);
		}
		return Normalized;
	}

	void ReflectShaderStage(ID3DBlob* Blob, FShaderReflectionInfo& OutReflection)
	{
		if (!Blob)
		{
			return;
		}

		ID3D11ShaderReflection* Reflector = nullptr;
		HRESULT Hr = D3DReflect(
			Blob->GetBufferPointer(),
			Blob->GetBufferSize(),
			IID_ID3D11ShaderReflection,
			reinterpret_cast<void**>(&Reflector));

		if (FAILED(Hr) || !Reflector)
		{
			return;
		}

		D3D11_SHADER_DESC ShaderDesc = {};
		Reflector->GetDesc(&ShaderDesc);

		for (UINT i = 0; i < ShaderDesc.BoundResources; ++i)
		{
			D3D11_SHADER_INPUT_BIND_DESC BindDesc = {};
			Reflector->GetResourceBindingDesc(i, &BindDesc);

			if (BindDesc.Type == D3D_SIT_TEXTURE)
			{
				OutReflection.TextureBindSlots[BindDesc.Name] = BindDesc.BindPoint;
			}
		}

		for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i)
		{
			ID3D11ShaderReflectionConstantBuffer* ConstantBuffer = Reflector->GetConstantBufferByIndex(i);

			D3D11_SHADER_BUFFER_DESC BufferDesc = {};
			ConstantBuffer->GetDesc(&BufferDesc);

			uint32 BufferBindPoint = 0;
			bool bFoundBindPoint = false;
			for (UINT j = 0; j < ShaderDesc.BoundResources; ++j)
			{
				D3D11_SHADER_INPUT_BIND_DESC ResDesc = {};
				Reflector->GetResourceBindingDesc(j, &ResDesc);

				if (ResDesc.Type == D3D_SIT_CBUFFER && strcmp(ResDesc.Name, BufferDesc.Name) == 0)
				{
					BufferBindPoint = ResDesc.BindPoint;
					bFoundBindPoint = true;
					break;
				}
			}

			if (!bFoundBindPoint)
			{
				continue;
			}

			for (UINT j = 0; j < BufferDesc.Variables; ++j)
			{
				ID3D11ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(j);

				D3D11_SHADER_VARIABLE_DESC VarDesc = {};
				Variable->GetDesc(&VarDesc);

				FShaderVariableReflection Info;
				Info.BufferSlot = BufferBindPoint;
				Info.Offset = VarDesc.StartOffset;
				Info.Size = VarDesc.Size;

				OutReflection.Variables[VarDesc.Name] = Info;
			}

			if (BufferBindPoint == 2)
			{
				OutReflection.ConstantBufferSize = BufferDesc.Size;
			}
		}

		Reflector->Release();
	}

	// VS Reflection 결과로 InputLayout을 생성합니다.
	// Fullscreen Pass처럼 SV_VertexID만 쓰는 VS는 입력 파라미터가 없으므로 InputLayout 없이 통과합니다.
	bool BuildInputLayoutFromReflection(ID3DBlob* VSBlob, ID3D11Device* Device, ID3D11InputLayout** OutInputLayout)
	{
		if (!VSBlob || !Device || !OutInputLayout)
		{
			return false;
		}

		ID3D11ShaderReflection* Reflector = nullptr;
		HRESULT Hr = D3DReflect(
			VSBlob->GetBufferPointer(),
			VSBlob->GetBufferSize(),
			IID_ID3D11ShaderReflection,
			reinterpret_cast<void**>(&Reflector));

		if (FAILED(Hr) || !Reflector)
		{
			return false;
		}

		D3D11_SHADER_DESC ShaderDesc = {};
		Reflector->GetDesc(&ShaderDesc);

		TArray<D3D11_INPUT_ELEMENT_DESC> InputElements;
		for (UINT i = 0; i < ShaderDesc.InputParameters; ++i)
		{
			D3D11_SIGNATURE_PARAMETER_DESC ParamDesc = {};
			Reflector->GetInputParameterDesc(i, &ParamDesc);

			D3D11_INPUT_ELEMENT_DESC ElementDesc = {};
			ElementDesc.SemanticName = ParamDesc.SemanticName;
			ElementDesc.SemanticIndex = ParamDesc.SemanticIndex;
			ElementDesc.InputSlot = 0;
			ElementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			ElementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			ElementDesc.InstanceDataStepRate = 0;
			ElementDesc.Format = GetReflectedDXGIFormat(ParamDesc);

			InputElements.push_back(ElementDesc);
		}

		bool bCreated = false;
		if (!InputElements.empty())
		{
			bCreated = SUCCEEDED(Device->CreateInputLayout(
				InputElements.data(),
				static_cast<UINT>(InputElements.size()),
				VSBlob->GetBufferPointer(),
				VSBlob->GetBufferSize(),
				OutInputLayout));
		}
		else
		{
			*OutInputLayout = nullptr;
			bCreated = true;
		}

		Reflector->Release();
		return bCreated;
	}

	bool BuildInputLayoutFromDesc(
		const FVertexLayoutDesc& VertexLayout,
		ID3DBlob* VSBlob,
		ID3D11Device* Device,
		ID3D11InputLayout** OutInputLayout)
	{
		if (!VSBlob || !Device || !OutInputLayout)
		{
			return false;
		}

		if (VertexLayout.Elements.empty())
		{
			*OutInputLayout = nullptr;
			return true;
		}

		TArray<D3D11_INPUT_ELEMENT_DESC> InputElements;
		InputElements.reserve(VertexLayout.Elements.size());
		for (const FVertexElementDesc& Element : VertexLayout.Elements)
		{
			D3D11_INPUT_ELEMENT_DESC ElementDesc = {};
			ElementDesc.SemanticName = Element.SemanticName.c_str();
			ElementDesc.SemanticIndex = Element.SemanticIndex;
			ElementDesc.Format = Element.Format;
			ElementDesc.InputSlot = Element.InputSlot;
			ElementDesc.AlignedByteOffset = Element.AlignedByteOffset;
			ElementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			ElementDesc.InstanceDataStepRate = 0;
			InputElements.push_back(ElementDesc);
		}

		return SUCCEEDED(Device->CreateInputLayout(
			InputElements.data(),
			static_cast<UINT>(InputElements.size()),
			VSBlob->GetBufferPointer(),
			VSBlob->GetBufferSize(),
			OutInputLayout));
	}
}

FVertexShader* FShaderResourceCache::GetOrCreateVertexShader(
	const FShaderStageKey& Key,
	const D3D_SHADER_MACRO* Defines,
	ID3D11Device* Device,
	const FVertexLayoutDesc* VertexLayout)
{
	if (!Device)
	{
		return nullptr;
	}

	// 같은 StageKey는 한 번만 컴파일하고 이후에는 캐시된 Stage를 재사용합니다.
	const FShaderStageKey NormalizedKey = NormalizeVertexStageKey(Key, VertexLayout);
	auto It = VertexShaders.find(NormalizedKey);
	if (It != VertexShaders.end())
	{
		return It->second;
	}

	FShaderCompileResult CompileResult = FShaderCompiler::CompileFromFile(
		NormalizedKey.FilePath,
		NormalizedKey.EntryPoint,
		NormalizedKey.Target.empty() ? FString("vs_5_0") : NormalizedKey.Target,
		Defines,
		NormalizedKey.PermutationKey);

	if (!CompileResult.bSuccess)
	{
		UE_LOG_ERROR("Failed to compile vertex shader stage: %s:%s\n%s",
			NormalizedKey.FilePath.c_str(),
			NormalizedKey.EntryPoint.c_str(),
			CompileResult.ErrorMessage.c_str());
		return nullptr;
	}

	FVertexShader* NewShader = new FVertexShader();
	HRESULT Hr = Device->CreateVertexShader(
		CompileResult.Blob->GetBufferPointer(),
		CompileResult.Blob->GetBufferSize(),
		nullptr,
		&NewShader->Shader);

	if (FAILED(Hr))
	{
		UE_LOG_ERROR("Failed to create vertex shader stage: %s:%s", NormalizedKey.FilePath.c_str(), NormalizedKey.EntryPoint.c_str());
		CompileResult.Blob->Release();
		delete NewShader;
		return nullptr;
	}

	const bool bHasExplicitLayout = VertexLayout && !VertexLayout->Elements.empty();
	const bool bInputLayoutCreated = bHasExplicitLayout
		? BuildInputLayoutFromDesc(*VertexLayout, CompileResult.Blob, Device, &NewShader->InputLayout)
		: BuildInputLayoutFromReflection(CompileResult.Blob, Device, &NewShader->InputLayout);
	if (!bInputLayoutCreated)
	{
		UE_LOG_ERROR("Failed to create vertex input layout: %s:%s", NormalizedKey.FilePath.c_str(), NormalizedKey.EntryPoint.c_str());
		CompileResult.Blob->Release();
		NewShader->Release();
		delete NewShader;
		return nullptr;
	}

	ReflectShaderStage(CompileResult.Blob, NewShader->Reflection);
	CompileResult.Blob->Release();
	VertexShaders[NormalizedKey] = NewShader;
	return NewShader;
}

FPixelShader* FShaderResourceCache::GetOrCreatePixelShader(const FShaderStageKey& Key, const D3D_SHADER_MACRO* Defines, ID3D11Device* Device)
{
	if (!Device)
	{
		return nullptr;
	}

	// PS Reflection은 Material Parameter / Texture Slot 바인딩에 사용됩니다.
	const FShaderStageKey NormalizedKey = NormalizeStageKey(Key);
	auto It = PixelShaders.find(NormalizedKey);
	if (It != PixelShaders.end())
	{
		return It->second;
	}

	FShaderCompileResult CompileResult = FShaderCompiler::CompileFromFile(
		NormalizedKey.FilePath,
		NormalizedKey.EntryPoint,
		NormalizedKey.Target.empty() ? FString("ps_5_0") : NormalizedKey.Target,
		Defines,
		NormalizedKey.PermutationKey);

	if (!CompileResult.bSuccess)
	{
		UE_LOG_ERROR("Failed to compile pixel shader stage: %s:%s\n%s",
			NormalizedKey.FilePath.c_str(),
			NormalizedKey.EntryPoint.c_str(),
			CompileResult.ErrorMessage.c_str());
		return nullptr;
	}

	FPixelShader* NewShader = new FPixelShader();
	HRESULT Hr = Device->CreatePixelShader(
		CompileResult.Blob->GetBufferPointer(),
		CompileResult.Blob->GetBufferSize(),
		nullptr,
		&NewShader->Shader);

	if (FAILED(Hr))
	{
		UE_LOG_ERROR("Failed to create pixel shader stage: %s:%s", NormalizedKey.FilePath.c_str(), NormalizedKey.EntryPoint.c_str());
		CompileResult.Blob->Release();
		delete NewShader;
		return nullptr;
	}

	ReflectShaderStage(CompileResult.Blob, NewShader->Reflection);
	CompileResult.Blob->Release();
	PixelShaders[NormalizedKey] = NewShader;
	return NewShader;
}

FShaderProgram* FShaderResourceCache::GetOrCreateProgram(
	const FShaderStageKey& VSKey,
	const FShaderStageKey& PSKey,
	const D3D_SHADER_MACRO* VSDefines,
	const D3D_SHADER_MACRO* PSDefines,
	ID3D11Device* Device,
	const FVertexLayoutDesc* VertexLayout)
{
	// Program은 VS/PS를 소유하지 않습니다.
	// Stage Cache에 있는 포인터를 조합해서 바인딩 단위만 만들어 둡니다.
	FShaderProgramKey ProgramKey = { NormalizeVertexStageKey(VSKey, VertexLayout), NormalizeStageKey(PSKey) };
	auto It = ShaderPrograms.find(ProgramKey);
	if (It != ShaderPrograms.end())
	{
		return It->second;
	}

	FVertexShader* VS = GetOrCreateVertexShader(ProgramKey.VS, VSDefines, Device, VertexLayout);
	FPixelShader* PS = GetOrCreatePixelShader(ProgramKey.PS, PSDefines, Device);
	if (!VS || !PS)
	{
		return nullptr;
	}

	FShaderProgram* Program = new FShaderProgram();
	Program->VS = VS;
	Program->PS = PS;

	ShaderPrograms[ProgramKey] = Program;
	return Program;
}

FComputeShader* FShaderResourceCache::GetComputeShader(const FString& Key) const
{
	auto It = ComputeShaders.find(Key);
	if (It != ComputeShaders.end()) return It->second;
	// Fallback: try normalized path for backward compatibility
	const FString NormalizedFilePath = FPaths::Normalize(Key);
	It = ComputeShaders.find(NormalizedFilePath);
	return (It != ComputeShaders.end()) ? It->second : nullptr;
}

bool FShaderResourceCache::LoadComputeShader(const FString& FilePath, const FString& EntryPoint,
	const D3D_SHADER_MACRO* Defines, const FString& Key, ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		return false;
	}

	const FString NormalizedFilePath = FPaths::Normalize(FilePath);
	const FString CacheKey = Key.empty() ? NormalizedFilePath : Key;

	FComputeShader* Shader = nullptr;
	auto It = ComputeShaders.find(CacheKey);
	if (It != ComputeShaders.end())
	{
		return true;
	}
	else
	{
		Shader = new FComputeShader();
		ComputeShaders[CacheKey] = Shader;
	}

	FShaderCompileResult CompileResult = FShaderCompiler::CompileFromFile(NormalizedFilePath, EntryPoint, "cs_5_0", Defines, 0);
	if (!CompileResult.bSuccess)
	{
		UE_LOG_ERROR("Compute Shader Compile Error (%s): %s", NormalizedFilePath.c_str(), CompileResult.ErrorMessage.c_str());
		return false;
	}

	TComPtr<ID3DBlob> CSBlob = CompileResult.Blob;

	HRESULT hr = Device->CreateComputeShader(CSBlob->GetBufferPointer(), CSBlob->GetBufferSize(), nullptr,
		&Shader->CS);
	if (FAILED(hr))
	{
		UE_LOG_ERROR("Failed to create compute shader: %s", NormalizedFilePath.c_str());
		return false;
	}

	return true;
}

void FShaderResourceCache::InvalidateShaderFile(const FString& FilePath)
{
	const FString NormalizedFilePath = FPaths::Normalize(FilePath);

	// Program이 Stage 포인터를 참조하므로 Program을 먼저 제거해야 dangling pointer를 피할 수 있습니다.
	for (auto It = ShaderPrograms.begin(); It != ShaderPrograms.end(); )
	{
		if (It->first.VS.FilePath == NormalizedFilePath || It->first.PS.FilePath == NormalizedFilePath)
		{
			delete It->second;
			It = ShaderPrograms.erase(It);
		}
		else
		{
			++It;
		}
	}

	for (auto It = VertexShaders.begin(); It != VertexShaders.end(); )
	{
		if (It->first.FilePath == NormalizedFilePath)
		{
			if (It->second)
			{
				It->second->Release();
				delete It->second;
			}
			It = VertexShaders.erase(It);
		}
		else
		{
			++It;
		}
	}

	for (auto It = PixelShaders.begin(); It != PixelShaders.end(); )
	{
		if (It->first.FilePath == NormalizedFilePath)
		{
			if (It->second)
			{
				It->second->Release();
				delete It->second;
			}
			It = PixelShaders.erase(It);
		}
		else
		{
			++It;
		}
	}

	UE_LOG("[ShaderHotReload] Invalidated shader stage cache: %s", NormalizedFilePath.c_str());
}

void FShaderResourceCache::Release()
{
	for (auto& [Key, Program] : ShaderPrograms)
	{
		delete Program;
	}
	ShaderPrograms.clear();

	for (auto& [Key, Shader] : VertexShaders)
	{
		if (Shader)
		{
			Shader->Release();
			delete Shader;
		}
	}
	VertexShaders.clear();

	for (auto& [Key, Shader] : PixelShaders)
	{
		if (Shader)
		{
			Shader->Release();
			delete Shader;
		}
	}
	PixelShaders.clear();

	for (auto& [Key, Shader] : ComputeShaders)
	{
		if (Shader)
		{
			Shader->Release();
			delete Shader;
		}
	}
	ComputeShaders.clear();
}
