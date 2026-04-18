#pragma once

/*
	Shader들을 관리하는 Class입니다.
	추후에 Geometry Shader, Compute Shader 등 다양한 Shader들을 관리하는 Class로 확장할 수 있습니다.
*/

#include "Render/Common/RenderTypes.h"

#include "Core/CoreTypes.h"
#include "Object/Object.h"

enum class EShaderLightPermutationKey : uint32
{
	Unlit = 0,
	Gouraud,
	Lambert,
	BlinnPhong,
};

struct FShaderVariableInfo
{
	uint32 BufferSlot = 0;
	uint32 Offset = 0;
	uint32 Size = 0;
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
		FShader* Target = &Permutations[PermutationKey];

		if (Target)
		{
			Context->IASetInputLayout(Target->InputLayout);
			Context->VSSetShader(Target->VS, nullptr, 0);
			Context->PSSetShader(Target->PS, nullptr, 0);
		}
	}

	void UpdateAndBindCBuffer(ID3D11DeviceContext* Context, const void* Data, uint32 Slot, uint32 Size, uint32 PermutationKey = 0)
	{
		FShader* Target = &Permutations[PermutationKey];
		if (!Target || !Target->ConstantBuffer)
		{
			return;
		}

		Context->UpdateSubresource(Target->ConstantBuffer, 0, nullptr, Data, 0, 0);
		Context->VSSetConstantBuffers(Slot, 1, &Target->ConstantBuffer);
		Context->PSSetConstantBuffers(Slot, 1, &Target->ConstantBuffer);
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

	void ReflectShader(ID3DBlob* ShaderBlob, ID3D11Device* Device, FShader& Target);

	uint32 GetCBufferSize() const { return CBufferSize; }

	FString FilePath;

private:
	TMap<uint32, FShader> Permutations;

	TMap<FString, uint32> TextureBindSlots;
	TMap<FString, FShaderVariableInfo> ShaderVariables;

	uint32 CBufferSize = 0;
};
