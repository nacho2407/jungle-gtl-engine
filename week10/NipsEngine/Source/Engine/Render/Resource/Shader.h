#pragma once

/*
	Shader들을 관리하는 Class입니다.
	추후에 Geometry Shader, Compute Shader 등 다양한 Shader들을 관리하는 Class로 확장할 수 있습니다.
*/

#include "Render/Common/RenderTypes.h"
#include "Render/Resource/ShaderHelper.h"

#include "Core/CoreTypes.h"
#include "Object/Object.h"

#include "Core/Logging/Log.h"

struct FShaderVariableInfo
{
	uint32 BufferSlot = 0;
	uint32 Offset = 0;
	uint32 Size = 0;
};

struct FShaderDefineDesc
{
	FString Name;
	FString Value;
};

struct FShaderPermutationDesc
{
	FString VSEntryPoint;
	FString PSEntryPoint;
	uint32 PermutationKey = 0;
	TArray<FShaderDefineDesc> Defines;
};

//	Shader Set
struct FShader
{
	ID3D11VertexShader* VS = nullptr;
	ID3D11PixelShader* PS = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;

	ID3D11Buffer* ConstantBuffer = nullptr;

	void Release()
	{
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
		if (ConstantBuffer)
		{
			ConstantBuffer->Release();
			ConstantBuffer = nullptr;
		}
	}
};

class UShader : public UObject
{
public:
	DECLARE_CLASS(UShader, UObject)
	~UShader() override
	{
		for (auto& Pair : Permutations)
		{
			Pair.second.Release();
		}
		Permutations.clear();
	}

	void SetPermutationDesc(uint32 Key, const FShaderPermutationDesc& Desc)
	{
		PermutationDescs[Key] = Desc;
	}
	bool HasPermutation(uint32 Key) const
	{
		return Permutations.contains(Key);
	}
	void AddPermutation(uint32 Key, const FShader& Data)
	{
		if (Permutations.contains(Key))
		{
			Permutations[Key].Release();
		}
		Permutations[Key] = Data;
	}

	void Bind(ID3D11DeviceContext* Context, uint32 PermutationKey = 0)
	{
		auto It = Permutations.find(PermutationKey);
		if (It == Permutations.end())
		{
			UE_LOG("Missing shader permutation: %s key=%u", FilePath.c_str(), PermutationKey);
			return;
		}

		FShader* Target = &It->second;

		if (Target)
		{
			Context->IASetInputLayout(Target->InputLayout);
			Context->VSSetShader(Target->VS, nullptr, 0);
			Context->PSSetShader(Target->PS, nullptr, 0);
		}
	}

	void UpdateAndBindCBuffer(ID3D11DeviceContext* Context, const void* Data, uint32 Slot, uint32 Size, uint32 PermutationKey = 0)
	{
		if (!Data || Size == 0) return;

		auto It = Permutations.find(PermutationKey);
		if (It == Permutations.end())
		{
			UE_LOG("Missing shader permutation: %s key=%u", FilePath.c_str(), PermutationKey);
			return;
		}

		FShader* Target = &It->second;

		if (Target->ConstantBuffer)
		{
			Context->UpdateSubresource(Target->ConstantBuffer, 0, nullptr, Data, 0, 0);
			Context->VSSetConstantBuffers(Slot, 1, &Target->ConstantBuffer);
			Context->PSSetConstantBuffers(Slot, 1, &Target->ConstantBuffer);
		}
	}

	int32 GetTextureBindSlot(const FString& Name) const
	{
		auto It = TextureBindSlots.find(Name);
		if (It != TextureBindSlots.end())
		{
			return It->second;
		}
		return -1;
	}
	bool GetShaderVariableInfo(const FString& Name, FShaderVariableInfo& OutInfo) const
	{
		auto It = ShaderVariables.find(Name);
		if (It != ShaderVariables.end())
		{
			OutInfo = It->second;
			return true;
		}
		return false;
	}

	void ReflectShader(ID3DBlob* ShaderBlob, ID3D11Device* Device, FShader& Target,
		TMap<FString, uint32>& OutTextureBindSlots, TMap<FString, FShaderVariableInfo>& OutShaderVariables, uint32& OutCBufferSize);
	
	bool EnsurePermutation(ID3D11Device* Device, uint32 PermutationKey);
	void Reload(ID3D11Device* Device);

	uint32 GetCBufferSize() const { return CBufferSize; }

	FString FilePath;

private:
	TMap<uint32, FShader> Permutations;
	TMap<uint32, FShaderPermutationDesc> PermutationDescs;
	uint32 Version = 0;

	TMap<FString, uint32> TextureBindSlots;
	TMap<FString, FShaderVariableInfo> ShaderVariables;

	uint32 CBufferSize = 0;
};
