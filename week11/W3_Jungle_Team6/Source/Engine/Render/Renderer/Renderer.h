#pragma once

/*
	실제 렌더링을 담당하는 Class 입니다. (Rendering 최상위 클래스)
*/

#include "Render/Common/RenderTypes.h"

#include "Render/Scene/RenderBus.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"
#include "Render/LineBatcher.h"
#include "Render/FontBatcher.h"

#include <cstddef>

class FRenderer
{
private:
	FD3DDevice Device;
	FRenderResources Resources;
	FLineBatcher LineBatcher;
	FFontBatcher FontBatcher;


	//	Primitive and Gizmo Input Layout
	D3D11_INPUT_ELEMENT_DESC PrimitiveInputLayout[2] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  static_cast<uint32>(offsetof(FVertex, Position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, static_cast<uint32>(offsetof(FVertex, Color)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

	//	Overlay Input Layout (Screen Quad)
	D3D11_INPUT_ELEMENT_DESC OverlayInputLayout[1] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

public:

private:
	void SetupRenderState(ERenderPass Pass, ID3D11DeviceContext* OutDeviceContext);
	void BindShaderByType(const FRenderCommand& InCmd, ID3D11DeviceContext* Context);
	EDepthStencilState GetDefaultDepthForPass(ERenderPass Pass) const;
	EBlendState GetDefaultBlendForPass(ERenderPass Pass) const;

	void DrawCommand(ID3D11DeviceContext* InDeviceContext, const FRenderCommand& InCommand);

public:
	void Create(HWND hWindow);
	void Release();

	void BeginFrame();
	void Render(const FRenderBus& InRenderBus, ERasterizerState InViewModeRasterizer = ERasterizerState::SolidBackCull);
	void RenderOverlay(const FRenderBus& InRenderBus);	//	반드시 따로 호출해야 함
	void EndFrame();

	void RenderPasses(const FRenderBus& InRenderBus, ID3D11DeviceContext* Context);
	void RenderEditorHelpers(const FRenderBus& InRenderBus, ID3D11DeviceContext* Context);
	void UpdateFrameBuffer(ID3D11DeviceContext* Context, const FMatrix& ViewMatrix, const FMatrix& ProjMatrix);
	void SetRenderSettings(const FRenderBus& InRenderBus);

	FD3DDevice& GetFD3DDevice() { return Device; }
	FRenderResources& GetResources() { return Resources; }
};

