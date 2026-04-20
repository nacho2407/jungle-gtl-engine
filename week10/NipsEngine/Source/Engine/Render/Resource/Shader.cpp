#include "Shader.h"

DEFINE_CLASS(UShader, UObject)

void FShader::Release()
{
	for (auto& Pair : ConstantBuffers)
	{
		if (Pair.second)
		{
			Pair.second->Release();
			Pair.second = nullptr;
		}
	}
	ConstantBuffers.clear();
	ConstantBufferSizes.clear();

	if (VS)
	{
		VS->Release();
		VS = nullptr;
	}
	if (PS)
	{
		PS->Release();
		PS = nullptr;
	}
	if (InputLayout)
	{
		InputLayout->Release();
		InputLayout = nullptr;
	}
}

UShader::~UShader()
{
	for (auto& Pair : Permutations)
	{
		Pair.second.Release();
	}
	Permutations.clear();
}

void UShader::AddPermutation(uint32 Key, const FShader& Data)
{
	if (Permutations.contains(Key))
	{
		Permutations[Key].Release();
	}
	Permutations[Key] = Data;
}

void UShader::Bind(ID3D11DeviceContext* Context, uint32 PermutationKey)
{
	auto It = Permutations.find(PermutationKey);
	if (It == Permutations.end())
	{
		UE_LOG("[%s] Shader permutation with key %u not found!", FilePath.c_str(), PermutationKey);
		return;
	}

	CurrentPermutation = &It->second;

	Context->IASetInputLayout(CurrentPermutation->InputLayout);
	Context->VSSetShader(CurrentPermutation->VS, nullptr, 0);
	Context->PSSetShader(CurrentPermutation->PS, nullptr, 0);
}

void UShader::UpdateAndBindCBuffer(ID3D11DeviceContext* Context, const void* Data, uint32 Slot, uint32 Size)
{
	if (!CurrentPermutation) return;

	auto It = CurrentPermutation->ConstantBuffers.find(Slot);
	if (It == CurrentPermutation->ConstantBuffers.end())
	{
		UE_LOG("[%s] Constant buffer slot %u not found in current permutation.", FilePath.c_str(), Slot);
		return;
	}

	ID3D11Buffer* ConstantBuffer = It->second;
	D3D11_MAPPED_SUBRESOURCE MappedResource;
	HRESULT hr = Context->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource);
	if (FAILED(hr))
	{
		UE_LOG("[%s] Failed to map constant buffer for updating.", FilePath.c_str());
		return;
	}

	std::memcpy(MappedResource.pData, Data, Size);
	Context->Unmap(ConstantBuffer, 0);

	Context->VSSetConstantBuffers(Slot, 1, &ConstantBuffer);
	Context->PSSetConstantBuffers(Slot, 1, &ConstantBuffer);
}

bool UShader::GetShaderVariableInfo(const FString& Name, FShaderVariableInfo& OutInfo) const
{
	if (!CurrentPermutation) return false;

	auto It = CurrentPermutation->Variables.find(Name);
	if (It != CurrentPermutation->Variables.end())
	{
		OutInfo = It->second;
		return true;
	}
	return false;
}

int32 UShader::GetTextureBindSlot(const FString& Name) const
{
	if (!CurrentPermutation) return -1;

	auto It = CurrentPermutation->TextureSlots.find(Name);
	if (It != CurrentPermutation->TextureSlots.end())
	{
		return It->second;
	}
	return -1;
}

void UShader::ReflectShader(ID3DBlob* ShaderBlob, ID3D11Device* Device, FShader& Target)
{
	if (!ShaderBlob || !Device) return;

	Target.Variables.clear();
	Target.TextureSlots.clear();
	Target.ConstantBuffers.clear();
	Target.ConstantBufferSizes.clear();

	ID3D11ShaderReflection* Reflector = nullptr;
	HRESULT hr = D3DReflect(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(),
		IID_PPV_ARGS(&Reflector));

	if (FAILED(hr) || !Reflector) return;

	D3D11_SHADER_DESC ShaderDesc;
	Reflector->GetDesc(&ShaderDesc);

	for (UINT i = 0; i < ShaderDesc.BoundResources; ++i)
	{
		D3D11_SHADER_INPUT_BIND_DESC BindDesc;
		Reflector->GetResourceBindingDesc(i, &BindDesc);

		if (BindDesc.Type == D3D_SIT_TEXTURE)
		{
			Target.TextureSlots[BindDesc.Name] = BindDesc.BindPoint;
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

		if (BufferBindPoint == 0 || BufferBindPoint == 1) continue;

		for (UINT j = 0; j < BufferDesc.Variables; ++j)
		{
			ID3D11ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(j);
			D3D11_SHADER_VARIABLE_DESC VarDesc;
			Variable->GetDesc(&VarDesc);

			FShaderVariableInfo Info;
			Info.BufferSlot = BufferBindPoint;
			Info.Offset = VarDesc.StartOffset;
			Info.Size = VarDesc.Size;

			Target.Variables[VarDesc.Name] = Info;
		}

		uint32 AlignedSize = (BufferDesc.Size + 15) & ~15;

		D3D11_BUFFER_DESC CBufferDesc = {};
		CBufferDesc.ByteWidth = AlignedSize;
		CBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		CBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		CBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		ID3D11Buffer* NewBuffer = nullptr;
		HRESULT hr = Device->CreateBuffer(&CBufferDesc, nullptr, &NewBuffer);
		if (FAILED(hr))
		{
			UE_LOG("[%s] Failed to create constant buffer for reflection.", FilePath.c_str());
			continue;
		}

		Target.ConstantBuffers[BufferBindPoint] = NewBuffer;
		Target.ConstantBufferSizes[BufferBindPoint] = AlignedSize;
	}

	Reflector->Release();
}