#pragma once

/*
	Shader들을 관리하는 Class입니다.
	추후에 Geometry Shader, Compute Shader 등 다양한 Shader들을 관리하는 Class로 확장할 수 있습니다.
*/

#include "Render/Common/RenderTypes.h"

#include "Core/CoreTypes.h"

//	Shader Set
class FShader
{
private:
	TComPtr<ID3D11VertexShader> VertexShader;
	TComPtr<ID3D11PixelShader> PixelShader;
	TComPtr<ID3D11InputLayout> InputLayout;

public:

private:

public:
	void Create(ID3D11Device* InDevice, const wchar_t* InFilePath, const char* InVSEntryPoint, const char* InPSEntryPoint,
		const D3D11_INPUT_ELEMENT_DESC* InInputElements, uint32 InInputElementCount);
	void Release();

	void Bind(ID3D11DeviceContext* InDeviceContext) const;
};



