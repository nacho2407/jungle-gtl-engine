#include "Shader.h"

DEFINE_CLASS(UShader, UObject)

void UShader::ReflectShader(ID3DBlob* ShaderBlob)
{
	if (!ShaderBlob) return;

	ID3D11ShaderReflection* Reflector = nullptr;
	HRESULT hr = D3DReflect(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize(),
		IID_ID3D11ShaderReflection, (void**)&Reflector);

	if (FAILED(hr) || !Reflector) return;

	D3D11_SHADER_DESC ShaderDesc;
	Reflector->GetDesc(&ShaderDesc);

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
		for (UINT j = 0; j < ShaderDesc.BoundResources; ++j)
		{
			D3D11_SHADER_INPUT_BIND_DESC ResDesc;
			Reflector->GetResourceBindingDesc(j, &ResDesc);

			if (ResDesc.Type == D3D_SIT_CBUFFER && strcmp(ResDesc.Name, BufferDesc.Name) == 0)
			{
				BufferBindPoint = ResDesc.BindPoint;
				break;
			}
		}

		for (UINT j = 0; j < BufferDesc.Variables; ++j)
		{
			ID3D11ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(j);

			D3D11_SHADER_VARIABLE_DESC VarDesc;
			Variable->GetDesc(&VarDesc);

			FShaderVariableInfo Info;
			Info.BufferSlot = i;
			Info.Offset = VarDesc.StartOffset;
			Info.Size = VarDesc.Size;

			ShaderVariables[VarDesc.Name] = Info;
		}
	}

	Reflector->Release();
}