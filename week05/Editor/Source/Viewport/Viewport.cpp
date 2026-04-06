#include "Viewport.h"

#include "EditorViewportClient.h"
#include "Core/Engine.h"
#include "Scene/Scene.h"

namespace
{
	void ReleaseIfValid(IUnknown*& Resource)
	{
		if (Resource)
		{
			Resource->Release();
			Resource = nullptr;
		}
	}
}

FViewport::~FViewport()
{
	Release();
}

void FViewport::SetRect(const FRect& InRect)
{
	Rect = InRect;
}

void FViewport::EnsureResources(ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		return;
	}

	if (!Rect.IsValid())
	{
		Release();
		return;
	}

	if (RenderTargetView && ShaderResourceView && DepthStencilView &&
		ResourceWidth == Rect.Width && ResourceHeight == Rect.Height)
	{
		return;
	}

	Release();

	D3D11_TEXTURE2D_DESC ColorDesc = {};
	ColorDesc.Width = Rect.Width;
	ColorDesc.Height = Rect.Height;
	ColorDesc.MipLevels = 1;
	ColorDesc.ArraySize = 1;
	ColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ColorDesc.SampleDesc.Count = 1;
	ColorDesc.Usage = D3D11_USAGE_DEFAULT;
	ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&ColorDesc, nullptr, &RenderTargetTexture)))
	{
		Release();
		return;
	}

	if (FAILED(Device->CreateRenderTargetView(RenderTargetTexture, nullptr, &RenderTargetView)))
	{
		Release();
		return;
	}

	if (FAILED(Device->CreateShaderResourceView(RenderTargetTexture, nullptr, &ShaderResourceView)))
	{
		Release();
		return;
	}

	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = Rect.Width;
	DepthDesc.Height = Rect.Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthStencilTexture)))
	{
		Release();
		return;
	}

	D3D11_DEPTH_STENCIL_VIEW_DESC DepthViewDesc = {};
	DepthViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DepthViewDesc.Texture2D.MipSlice = 0;
	if (FAILED(Device->CreateDepthStencilView(DepthStencilTexture, &DepthViewDesc, &DepthStencilView)))
	{
		Release();
		return;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC DepthSRVDesc = {};
	DepthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	DepthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	DepthSRVDesc.Texture2D.MostDetailedMip = 0;
	DepthSRVDesc.Texture2D.MipLevels = 1;
	if (FAILED(Device->CreateShaderResourceView(DepthStencilTexture, &DepthSRVDesc, &DepthShaderResourceView)))
	{
		Release();
		return;
	}

	ResourceWidth = Rect.Width;
	ResourceHeight = Rect.Height;
}

void FViewport::Release()
{
	IUnknown* Resource = reinterpret_cast<IUnknown*>(DepthShaderResourceView);
	ReleaseIfValid(Resource);
	DepthShaderResourceView = nullptr;

	Resource = reinterpret_cast<IUnknown*>(DepthStencilView);
	ReleaseIfValid(Resource);
	DepthStencilView = nullptr;

	Resource = reinterpret_cast<IUnknown*>(DepthStencilTexture);
	ReleaseIfValid(Resource);
	DepthStencilTexture = nullptr;

	Resource = reinterpret_cast<IUnknown*>(ShaderResourceView);
	ReleaseIfValid(Resource);
	ShaderResourceView = nullptr;

	Resource = reinterpret_cast<IUnknown*>(RenderTargetView);
	ReleaseIfValid(Resource);
	RenderTargetView = nullptr;

	Resource = reinterpret_cast<IUnknown*>(RenderTargetTexture);
	ReleaseIfValid(Resource);
	RenderTargetTexture = nullptr;

	ResourceWidth = 0;
	ResourceHeight = 0;
}
