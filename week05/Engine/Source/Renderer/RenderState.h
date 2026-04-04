#pragma once

#include "EngineAPI.h"
#include "Types/CoreTypes.h"
#include <d3d11.h>
#include <memory>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct FRasterizerStateOption {
	bool isDirty = true;	// 최초 1회 초기화 보장
	D3D11_FILL_MODE FillMode = D3D11_FILL_SOLID;	// 기본값 Fill
	D3D11_CULL_MODE CullMode = D3D11_CULL_BACK;
	bool DepthClipEnable = true;
	int DepthBias = 0;

	uint32 ToKey() const {
		uint32 key = 0;
		key |= (static_cast<uint32>(FillMode) & 0x7);				// 3 bit
		key |= (static_cast<uint32>(CullMode) & 0x7) << 3;			// 3 bit
		key |= (DepthClipEnable ? 1 : 0) << 6;						// 1 bit
		key |= (static_cast<uint32>(DepthBias) & 0xFFFF) << 7;		// 16 bit
		return key;
	}
};

struct ENGINE_API FRasterizerState
{
public:
	~FRasterizerState() = default;

	static std::shared_ptr<FRasterizerState> Create(
		ID3D11Device* InDevice, const FRasterizerStateOption& InOption );

	void Bind(ID3D11DeviceContext* InDeviceContext) const;
	void Release();

private:
	FRasterizerState() = default;
	ComPtr<ID3D11RasterizerState> State = nullptr;
};

struct FDepthStencilStateOption {
	bool isDirty = true;	// 최초 1회 초기화 보장
	bool DepthEnable = true;
	D3D11_DEPTH_WRITE_MASK DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	D3D11_COMPARISON_FUNC DepthFunc = D3D11_COMPARISON_LESS;
	bool StencilEnable = false;
	uint8 StencilReadMask = 0;
	uint8 StencilWriteMask = 0;

	uint32 ToKey() const {
		uint32 key = 0;
		key |= (DepthEnable ? 1 : 0);                                  // bit 0      : DepthEnable
		key |= (static_cast<uint32>(DepthWriteMask) & 0x1) << 1;       // bit 1      : DepthWriteMask
		key |= (static_cast<uint32>(DepthFunc) & 0xF) << 2;            // bits 2-5   : DepthFunc
		key |= (StencilEnable ? 1 : 0) << 6;                           // bit 6      : StencilEnable
		key |= (static_cast<uint32>(StencilReadMask) & 0xFF) << 7;     // bits 7-14  : StencilReadMask
		key |= (static_cast<uint32>(StencilWriteMask) & 0xFF) << 15;   // bits 15-22 : StencilWriteMask
		return key;
	}
};

struct FDepthStencilState {
	public:
		~FDepthStencilState() = default;

		static std::shared_ptr<FDepthStencilState> Create(
			ID3D11Device* InDevice, const FDepthStencilStateOption& InOption);

		void Bind(ID3D11DeviceContext* InDeviceContext) const;
		void Release();

	private:
		FDepthStencilState() = default;
		ComPtr<ID3D11DepthStencilState> State = nullptr;
};

struct FBlendStateOption {
	bool isDirty = true;
	bool BlendEnable = false;
	D3D11_BLEND SrcBlend = D3D11_BLEND_ONE;
	D3D11_BLEND DestBlend = D3D11_BLEND_ZERO;
	D3D11_BLEND_OP BlendOp = D3D11_BLEND_OP_ADD;
	D3D11_BLEND SrcBlendAlpha = D3D11_BLEND_ONE;
	D3D11_BLEND DestBlendAlpha = D3D11_BLEND_ZERO;
	D3D11_BLEND_OP BlendOpAlpha = D3D11_BLEND_OP_ADD;
	uint8 RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	uint32 ToKey() const {
		uint32 key = 0;
		key |= (BlendEnable ? 1 : 0);                                      // bit 0      : BlendEnable
		key |= (static_cast<uint32>(SrcBlend) & 0x1F) << 1;               // bits 1-5   : SrcBlend
		key |= (static_cast<uint32>(DestBlend) & 0x1F) << 6;              // bits 6-10  : DestBlend
		key |= (static_cast<uint32>(BlendOp) & 0x7) << 11;                // bits 11-13 : BlendOp
		key |= (static_cast<uint32>(SrcBlendAlpha) & 0x1F) << 14;         // bits 14-18 : SrcBlendAlpha
		key |= (static_cast<uint32>(DestBlendAlpha) & 0x1F) << 19;        // bits 19-23 : DestBlendAlpha
		key |= (static_cast<uint32>(BlendOpAlpha) & 0x7) << 24;           // bits 24-26 : BlendOpAlpha
		key |= (static_cast<uint32>(RenderTargetWriteMask) & 0xF) << 27;  // bits 27-30 : RenderTargetWriteMask
		return key;
	}
};

struct FBlendState {
public:
	~FBlendState() = default;

	static std::shared_ptr<FBlendState> Create(
		ID3D11Device* InDevice, const FBlendStateOption& InOption);

	void Bind(ID3D11DeviceContext* InDeviceContext) const;
	void Release();

private:
	FBlendState() = default;
	ComPtr<ID3D11BlendState> State = nullptr;
};
