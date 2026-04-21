#include "Shader.h"

DEFINE_CLASS(UShader, UObject)

DXGI_FORMAT GetDXGIFormat(const D3D11_SIGNATURE_PARAMETER_DESC& ParamDesc)
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

void UShader::ReflectShader(ID3DBlob* ShaderBlob, ID3D11Device* Device, FShader& Target)
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

	Reflector->Release();
}