
#pragma once

#include "Render/Common/RenderTypes.h"
#include "Render/Scene/RenderBus.h"
#include "Render/Device/D3DDevice.h"
#include "Render/Resource/RenderResources.h"

#include <cstddef>
#include <functional>

// Render pass configuration encapsulating all required states
struct FRenderPassConfig
{
	EDepthStencilState DepthState;
	EBlendState BlendState;
	ERasterizerState RasterizerState;
	D3D11_PRIMITIVE_TOPOLOGY Topology;
	EShaderType ShaderType;
	const char* PassName;
};

// Pass execution function type
using FPassExecuteFunc = std::function<void()>;

class FRenderPipeline
{
public:
	enum class EPass
	{
		Component = 0,
		EditorAxis = 1,
		EditorGrid = 2,
		SelectionOutline = 3,
		GizmoDepthLess = 4,
		Count
	};

private:
	// Static pass configurations
	static const FRenderPassConfig PassConfigs[(int)EPass::Count];

	FD3DDevice* Device = nullptr;
	FRenderResources* Resources = nullptr;

public:
	FRenderPipeline(FD3DDevice* InDevice, FRenderResources* InResources)
		: Device(InDevice), Resources(InResources)
	{
	}

	// Execute a single pass with proper state setup
	void ExecutePass(EPass PassType, const FPassExecuteFunc& ExecuteFunc)
	{
		if (!Device || !Resources)
		{
			return;
		}

		const FRenderPassConfig& Config = PassConfigs[(int)PassType];

		ID3D11DeviceContext* Context = Device->GetDeviceContext();

		// Apply all required states
		Device->SetDepthStencilState(Config.DepthState);
		Device->SetBlendState(Config.BlendState);
		Device->SetRasterizerState(Config.RasterizerState);
		Context->IASetPrimitiveTopology(Config.Topology);

		// Execute the pass
		if (ExecuteFunc)
		{
			ExecuteFunc();
		}
	}

	// Get the configuration for a specific pass
	static const FRenderPassConfig& GetPassConfig(EPass PassType)
	{
		return PassConfigs[(int)PassType];
	}
};
