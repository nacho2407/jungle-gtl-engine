#include "ObjViewerViewportSurface.h"

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

FObjViewerViewportSurface::~FObjViewerViewportSurface()
{
	Release();
}

void FObjViewerViewportSurface::SetSize(int32 InWidth, int32 InHeight)
{
	Width = InWidth;
	Height = InHeight;
}

void FObjViewerViewportSurface::EnsureResources(ID3D11Device* Device)
{
	if (Device == nullptr)
	{
		return;
	}

	if (Width <= 0 || Height <= 0)
	{
		Release();
		return;
	}

	if (RenderTargetView && ShaderResourceView && DepthStencilView &&
		ResourceWidth == static_cast<uint32>(Width) &&
		ResourceHeight == static_cast<uint32>(Height))
	{
		return;
	}

	Release();

	D3D11_TEXTURE2D_DESC ColorDesc = {};
	ColorDesc.Width = static_cast<uint32>(Width);
	ColorDesc.Height = static_cast<uint32>(Height);
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
	DepthDesc.Width = static_cast<uint32>(Width);
	DepthDesc.Height = static_cast<uint32>(Height);
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthStencilTexture)))
	{
		Release();
		return;
	}

	if (FAILED(Device->CreateDepthStencilView(DepthStencilTexture, nullptr, &DepthStencilView)))
	{
		Release();
		return;
	}

	ResourceWidth = static_cast<uint32>(Width);
	ResourceHeight = static_cast<uint32>(Height);
}

void FObjViewerViewportSurface::Release()
{
	IUnknown* Resource = reinterpret_cast<IUnknown*>(DepthStencilView);
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

bool FObjViewerViewportSurface::IsValid() const
{
	return Width > 0 && Height > 0 &&
		RenderTargetView != nullptr &&
		ShaderResourceView != nullptr &&
		DepthStencilView != nullptr;
}
