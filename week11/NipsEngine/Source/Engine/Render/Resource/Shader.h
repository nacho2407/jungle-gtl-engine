#pragma once

/*
	Shader들을 관리하는 Class입니다.
	추후에 Geometry Shader, Compute Shader 등 다양한 Shader들을 관리하는 Class로 확장할 수 있습니다.
*/

#include "Render/Common/RenderTypes.h"

#include "Core/CoreTypes.h"
#include "Object/Object.h"
#include "UI/EditorConsoleWidget.h"

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

	TMap<uint32, ID3D11Buffer*> ConstantBuffers;
	TMap<uint32, uint32> ConstantBufferSizes;

	TMap<FString, FShaderVariableInfo> Variables;
	TMap<FString, uint32> TextureSlots;

	void Release();
};

class UShader : public UObject
{
public:
	DECLARE_CLASS(UShader, UObject)
	~UShader() override;

	void AddPermutation(uint32 Key, const FShader& Data);
	
	void Bind(ID3D11DeviceContext* Context, uint32 PermutationKey = 0);

	void UpdateAndBindCBuffer(ID3D11DeviceContext* Context, const void* Data, uint32 Slot, uint32 Size);

	const TMap<uint32, uint32> GetCBufferSizes() const { return CurrentPermutation ? CurrentPermutation->ConstantBufferSizes : TMap<uint32, uint32>(); }
	bool GetShaderVariableInfo(const FString& Name, FShaderVariableInfo& OutInfo) const;
	int32 GetTextureBindSlot(const FString& Name) const;

	void ReflectShader(ID3DBlob* ShaderBlob, ID3D11Device* Device, FShader& Target);

	FString FilePath;

private:
	TMap<uint32, FShader> Permutations;
	FShader* CurrentPermutation = nullptr;
};
