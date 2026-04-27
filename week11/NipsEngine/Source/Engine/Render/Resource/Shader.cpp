#include "Shader.h"

#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "Render/Resource/ShaderCompiler.h"

DEFINE_CLASS(UShader, UObject)

static DXGI_FORMAT GetDXGIFormat(const D3D11_SIGNATURE_PARAMETER_DESC& ParamDesc)
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

static TArray<D3D_SHADER_MACRO> BuildShaderMacros(const TArray<FShaderDefineDesc>& Defines)
{
	static std::vector<D3D_SHADER_MACRO> Macros;
	Macros.clear();
	for (const auto& Define : Defines)
	{
		D3D_SHADER_MACRO Macro = {};
		Macro.Name = Define.Name.c_str();
		Macro.Definition = Define.Value.c_str();
		Macros.push_back(Macro);
	}
	D3D_SHADER_MACRO EndMacro = {};
	Macros.push_back(EndMacro);

	return Macros;
}

static void ReleasePermutationMap(TMap<uint32, FShader>& Permutations)
{
	for (auto& Pair : Permutations)
	{
		Pair.second.Release();
	}
	Permutations.clear();
}

static bool BuildFallbackPermutationDesc(const FString& FilePath, uint32 PermutationKey, FShaderPermutationDesc& OutDesc)
{
	const FString NormalizedUberLitPath = FPaths::Normalize("Shaders/UberLit.hlsl");
	if (FilePath != NormalizedUberLitPath)
	{
		return false;
	}

	OutDesc.VSEntryPoint = "mainVS";
	OutDesc.PSEntryPoint = "mainPS";
	OutDesc.PermutationKey = PermutationKey;

	TArray<D3D_SHADER_MACRO> Macros = FShaderHelper::BuildUberLitMacros(PermutationKey);
	for (const D3D_SHADER_MACRO& Macro : Macros)
	{
		if (!Macro.Name)
		{
			break;
		}
		OutDesc.Defines.push_back({ Macro.Name, Macro.Definition ? Macro.Definition : "" });
	}

	return true;
}

static bool CompileShaderPermutation(
	UShader& ShaderOwner,
	ID3D11Device* Device,
	const FShaderPermutationDesc& Desc,
	FShader& OutShader,
	TMap<FString, uint32>& OutTextureBindSlots,
	TMap<FString, FShaderVariableInfo>& OutShaderVariables,
	uint32& OutCBufferSize)
{
	if (!Device)
	{
		return false;
	}

	TComPtr<ID3DBlob> VSBlob;
	TComPtr<ID3DBlob> PSBlob;

	TArray<D3D_SHADER_MACRO> Macros = BuildShaderMacros(Desc.Defines);

	FShaderCompileResult VSResult = FShaderCompiler::CompileFromFile(
		ShaderOwner.FilePath, Desc.VSEntryPoint, "vs_5_0", Macros.data(), Desc.PermutationKey);
	if (!VSResult.bSuccess)
	{
		UE_LOG("Failed to compile vertex shader: %s key=%u", ShaderOwner.FilePath.c_str(), Desc.PermutationKey);
		return false;
	}
	VSBlob = VSResult.Blob;

	FShaderCompileResult PSResult = FShaderCompiler::CompileFromFile(
		ShaderOwner.FilePath, Desc.PSEntryPoint, "ps_5_0", Macros.data(), Desc.PermutationKey);
	if (!PSResult.bSuccess)
	{
		UE_LOG("Failed to compile pixel shader: %s key=%u", ShaderOwner.FilePath.c_str(), Desc.PermutationKey);
		return false;
	}
	PSBlob = PSResult.Blob;

	if (FAILED(Device->CreateVertexShader(VSBlob->GetBufferPointer(), VSBlob->GetBufferSize(), nullptr, &OutShader.VS)))
	{
		UE_LOG("Failed to create vertex shader: %s key=%u", ShaderOwner.FilePath.c_str(), Desc.PermutationKey);
		return false;
	}

	if (FAILED(Device->CreatePixelShader(PSBlob->GetBufferPointer(), PSBlob->GetBufferSize(), nullptr, &OutShader.PS)))
	{
		UE_LOG("Failed to create pixel shader: %s key=%u", ShaderOwner.FilePath.c_str(), Desc.PermutationKey);
		OutShader.Release();
		return false;
	}

	ShaderOwner.ReflectShader(VSBlob.Get(), Device, OutShader, OutTextureBindSlots, OutShaderVariables, OutCBufferSize);
	ShaderOwner.ReflectShader(PSBlob.Get(), Device, OutShader, OutTextureBindSlots, OutShaderVariables, OutCBufferSize);
	return true;
}

void UShader::ReflectShader(ID3DBlob* ShaderBlob, ID3D11Device* Device, FShader& Target,
	TMap<FString, uint32>& OutTextureBindSlots, TMap<FString, FShaderVariableInfo>& OutShaderVariables, uint32& OutCBufferSize)
{
	if (!ShaderBlob || !Device) return;

	ID3D11ShaderReflection* Reflector = nullptr;
	HRESULT hr = D3DReflect(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection, (void**)&Reflector);

	if (FAILED(hr) || !Reflector) return;

	D3D11_SHADER_DESC ShaderDesc;
	Reflector->GetDesc(&ShaderDesc);

	UINT ShaderType = D3D11_SHVER_GET_TYPE(ShaderDesc.Version);
	if (ShaderType == D3D11_SHVER_VERTEX_SHADER)
	{
		TArray<D3D11_INPUT_ELEMENT_DESC> InputElements;

		for (UINT i = 0; i < ShaderDesc.InputParameters; ++i)
		{
			D3D11_SIGNATURE_PARAMETER_DESC ParamDesc;
			Reflector->GetInputParameterDesc(i, &ParamDesc);

			D3D11_INPUT_ELEMENT_DESC ElementDesc = {};
			ElementDesc.SemanticName = ParamDesc.SemanticName;
			ElementDesc.SemanticIndex = ParamDesc.SemanticIndex;
			ElementDesc.InputSlot = 0;
			ElementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			ElementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
			ElementDesc.InstanceDataStepRate = 0;

			ElementDesc.Format = GetDXGIFormat(ParamDesc);

			InputElements.push_back(ElementDesc);
		}

		if (InputElements.size() > 0)
		{
			Device->CreateInputLayout(InputElements.data(), (UINT)InputElements.size(),
				ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(), &Target.InputLayout);
		}
	}

	for (UINT i = 0; i < ShaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC BindDesc;
		Reflector->GetResourceBindingDesc(i, &BindDesc);

		if (BindDesc.Type == D3D_SIT_TEXTURE)
		{
			TextureBindSlots[BindDesc.Name] = BindDesc.BindPoint;
		}
	}

	for (UINT i = 0; i < ShaderDesc.ConstantBuffers; ++i)
	{
		ID3D11ShaderReflectionConstantBuffer* ConstantBuffer = Reflector->GetConstantBufferByIndex(i);

		D3D11_SHADER_BUFFER_DESC BufferDesc;
		ConstantBuffer->GetDesc(&BufferDesc);

		uint32 BufferBindPoint = 0;
		bool bFoundBindPoint = false;
		for (UINT j = 0; j < ShaderDesc.BoundResources; ++j)
		{
			D3D11_SHADER_INPUT_BIND_DESC ResDesc;
			Reflector->GetResourceBindingDesc(j, &ResDesc);

			if (ResDesc.Type == D3D_SIT_CBUFFER && strcmp(ResDesc.Name, BufferDesc.Name) == 0)
			{
				BufferBindPoint = ResDesc.BindPoint;
				bFoundBindPoint = true;
				break;
			}
		}

		if (!bFoundBindPoint) continue;

		for (UINT j = 0; j < BufferDesc.Variables; ++j)
		{
			ID3D11ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(j);

			D3D11_SHADER_VARIABLE_DESC VarDesc;
			Variable->GetDesc(&VarDesc);

			FShaderVariableInfo Info;
			Info.BufferSlot = BufferBindPoint;
			Info.Offset = VarDesc.StartOffset;
			Info.Size = VarDesc.Size;

			ShaderVariables[VarDesc.Name] = Info;
		}

		// 머티리얼에서 사용하는 상수 버퍼 슬롯은 2번으로 고정되어 있음.
		// 해당 슬롯의 버퍼 크기를 셰이더의 메인 상수 버퍼 크기로 사용.
		if (BufferBindPoint == 2)
		{
			CBufferSize = BufferDesc.Size;
		}
	}

	if (CBufferSize > 0)
	{
		if (Target.ConstantBuffer)
		{
			Target.ConstantBuffer->Release();
			Target.ConstantBuffer = nullptr;
		}

		D3D11_BUFFER_DESC CBufferDesc = {};
		CBufferDesc.ByteWidth = CBufferSize;
		CBufferDesc.Usage = D3D11_USAGE_DEFAULT; // Dynamic 에서 Default로 변경 (UpdateSubresource 사용을 위함)
		CBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		CBufferDesc.CPUAccessFlags = 0;
		Device->CreateBuffer(&CBufferDesc, nullptr, &Target.ConstantBuffer);
	}

	OutTextureBindSlots = TextureBindSlots;
	OutShaderVariables = ShaderVariables;
	OutCBufferSize = CBufferSize;

	Reflector->Release();
}

bool UShader::EnsurePermutation(ID3D11Device* Device, uint32 PermutationKey)
{
	if (HasPermutation(PermutationKey))
	{
		return true;
	}

	FShaderPermutationDesc Desc;
	auto DescIt = PermutationDescs.find(PermutationKey);
	if (DescIt != PermutationDescs.end())
	{
		Desc = DescIt->second;
	}
	else if (!BuildFallbackPermutationDesc(FilePath, PermutationKey, Desc))
	{
		return false;
	}

	FShader NewShader;
	TMap<FString, uint32> NewTextureBindSlots = TextureBindSlots;
	TMap<FString, FShaderVariableInfo> NewShaderVariables = ShaderVariables;
	uint32 NewCBufferSize = CBufferSize;

	if (!CompileShaderPermutation(*this, Device, Desc, NewShader, NewTextureBindSlots, NewShaderVariables, NewCBufferSize))
	{
		return false;
	}

	PermutationDescs[PermutationKey] = Desc;
	AddPermutation(PermutationKey, NewShader);
	TextureBindSlots = std::move(NewTextureBindSlots);
	ShaderVariables = std::move(NewShaderVariables);
	CBufferSize = NewCBufferSize;

	return true;
}

void UShader::Reload(ID3D11Device* Device)
{
	if (!Device) return;

	TMap<uint32, FShader> NewPermutations;

	TMap<FString, uint32> NewTextureBindSlots;
	TMap<FString, FShaderVariableInfo> NewShaderVariables;
	uint32 NewCBufferSize = 0;

	for (const auto& [Key, Desc] : PermutationDescs)
	{
		FShader NewShader;
		if (!CompileShaderPermutation(*this, Device, Desc, NewShader, NewTextureBindSlots, NewShaderVariables, NewCBufferSize))
		{
			ReleasePermutationMap(NewPermutations);
			return;
		}

		if (CBufferSize > 0)
		{
			if (NewShader.ConstantBuffer)
			{
				NewShader.ConstantBuffer->Release();
				NewShader.ConstantBuffer = nullptr;
			}

			D3D11_BUFFER_DESC CBufferDesc = {};
			CBufferDesc.ByteWidth = CBufferSize;
			CBufferDesc.Usage = D3D11_USAGE_DEFAULT; // Dynamic 에서 Default로 변경 (UpdateSubresource 사용을 위함)
			CBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			CBufferDesc.CPUAccessFlags = 0;
			Device->CreateBuffer(&CBufferDesc, nullptr, &NewShader.ConstantBuffer);
		}

		NewPermutations[Key] = NewShader;
	}

	for (auto& Pair : Permutations)
	{
		Pair.second.Release();
	}

	Permutations = std::move(NewPermutations);
	TextureBindSlots = std::move(NewTextureBindSlots);
	ShaderVariables = std::move(NewShaderVariables);
	CBufferSize = NewCBufferSize;
	++Version;
	
	UE_LOG("Shader reloaded: %s (version %u)\n", FilePath.c_str(), Version);
}
