#pragma once
#include "UObject.h"
#include "d3d11.h"
#include "FMatrix.h"
struct RenderObjectConstants
{
	FMatrix Model;
	FMatrix View;
	FMatrix Projection;
};

class URenderObject : public UObject
{
	DECLARE_RTTI(URenderObject, UObject)
public:
	URenderObject();
	virtual ~URenderObject() override;

	void SetObject(ID3D11Buffer* VertexBuffer, ID3D11VertexShader* VS, ID3D11PixelShader* PS, ID3D11InputLayout* LO,
		UINT VertexCount, UINT Offset, UINT Stride)
	{
		this->VertexBuffer = VertexBuffer;
		this->VertexShader = VS;
		this->PixelShader = PS;
		this->InputLayout = LO;
		this->VertexCount = VertexCount;
		this->Offset = Offset;
		this->Stride = Stride;
	}
	void SetConstantBuffer(ID3D11Buffer* ConstantBuffer)
	{
		this->ConstantBuffer = ConstantBuffer;
	}
	void SetConstants(const RenderObjectConstants& Constants)
	{
		this->Constants = Constants;
	}


	void UpdateConstant(ID3D11DeviceContext* Context, const RenderObjectConstants* Constants)
	{
		if (ConstantBuffer) {
			D3D11_MAPPED_SUBRESOURCE constantbufferMSR;

			HRESULT hr = Context->Map(ConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD,
				0, &constantbufferMSR);

			if (SUCCEEDED(hr)) {
				memcpy(constantbufferMSR.pData, Constants, sizeof(RenderObjectConstants));
				Context->Unmap(ConstantBuffer, 0);
			}
		}
	}

	virtual void Render(ID3D11DeviceContext* Context)
	{
		Context->VSSetShader(VertexShader, nullptr, 0);
		Context->PSSetShader(PixelShader, nullptr, 0);
		Context->IASetInputLayout(InputLayout);
		UpdateConstant(Context, &Constants);

		Context->IASetVertexBuffers(0, 1, &VertexBuffer, &Stride, &Offset);
		if (ConstantBuffer)
		{
			Context->VSSetConstantBuffers(0, 1, &ConstantBuffer);
		}
		Context->Draw(VertexCount, 0);
	}

public:
	//2/2묶고
	ID3D11Buffer* VertexBuffer;
	ID3D11InputLayout* InputLayout;
	ID3D11VertexShader* VertexShader;
	ID3D11PixelShader* PixelShader;
	ID3D11Buffer* ConstantBuffer;
	RenderObjectConstants Constants;
	UINT VertexCount;
	UINT Offset;
	UINT Stride;
	//Texture
};