#pragma once

#include <d3d11.h>
#include "EngineTypes.h"
class FConstantBuffer {
public:
	TComPtr<ID3D11Buffer> Buffer = nullptr;
	UINT Slot = 0;

	void Create(ID3D11Device* Device, UINT ByteWidth, UINT InSlot)
	{
		Slot = InSlot;
		D3D11_BUFFER_DESC desc = {};
		desc.ByteWidth = (ByteWidth + 15) & ~15;
		desc.Usage = D3D11_USAGE_DYNAMIC;
		desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Device->CreateBuffer(&desc, nullptr, Buffer.GetAddressOf());
	}

	void Update(ID3D11DeviceContext* Context, const void* Data, UINT Size)
	{
		D3D11_MAPPED_SUBRESOURCE mappedResource;
		if (SUCCEEDED(Context->Map(Buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
		{
			memcpy(mappedResource.pData, Data, Size);
			Context->Unmap(Buffer.Get(), 0);
		}
	}

	void Release()
	{
		Buffer.Reset();
	}

	void Bind(ID3D11DeviceContext* Context)
	{
		Context->VSSetConstantBuffers(Slot, 1, Buffer.GetAddressOf());
		Context->PSSetConstantBuffers(Slot, 1, Buffer.GetAddressOf());
	}
};