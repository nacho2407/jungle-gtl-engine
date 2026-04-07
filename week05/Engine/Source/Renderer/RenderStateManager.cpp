#include "RenderStateManager.h"
#include "RenderState.h"

void FRenderStateManager::PrepareCommonStates()
{
	// 예: Solid/Wireframe x Cull None/Back/Front
	D3D11_FILL_MODE fills[] = { D3D11_FILL_SOLID, D3D11_FILL_WIREFRAME };
	D3D11_CULL_MODE culls[] = { D3D11_CULL_NONE, D3D11_CULL_FRONT, D3D11_CULL_BACK };

	for (auto f : fills) {
		for (auto c : culls) {
			FRasterizerStateOption opt;
			opt.FillMode = f;
			opt.CullMode = c;
			GetOrCreateRasterizerState(opt);
		}
	}

	// 기본 블렌드 상태 (No Blend)
	FBlendStateOption blendOpt;
	GetOrCreateBlendState(blendOpt);

	// 알파 블렌드 상태
	blendOpt.BlendEnable = true;
	blendOpt.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	blendOpt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	blendOpt.BlendOp = D3D11_BLEND_OP_ADD;
	blendOpt.SrcBlendAlpha = D3D11_BLEND_ONE;
	blendOpt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	blendOpt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	GetOrCreateBlendState(blendOpt);
}

std::shared_ptr<FRasterizerState> FRenderStateManager::GetOrCreateRasterizerState(const FRasterizerStateOption& opt)
{
	uint32 key = opt.ToKey();
	auto it = RasterizerStateMap.find(key);
	if (it != RasterizerStateMap.end()) {
		return it->second;
	}
	auto state = FRasterizerState::Create(Device, opt);
	RasterizerStateMap[key] = state;
	return state;
}

std::shared_ptr<FDepthStencilState> FRenderStateManager::GetOrCreateDepthStencilState(const FDepthStencilStateOption& opt)
{
	uint32 key = opt.ToKey();
	auto it = DepthStencilStateMap.find(key);
	if (it != DepthStencilStateMap.end()) {
		return it->second;
	}
	auto state = FDepthStencilState::Create(Device, opt);
	DepthStencilStateMap[key] = state;
	return state;
}

std::shared_ptr<FBlendState> FRenderStateManager::GetOrCreateBlendState(const FBlendStateOption& opt)
{
	uint32 key = opt.ToKey();
	auto it = BlendStateMap.find(key);
	if (it != BlendStateMap.end()) {
		return it->second;
	}
	auto state = FBlendState::Create(Device, opt);
	BlendStateMap[key] = state;
	return state;
}

void FRenderStateManager::BindState(std::shared_ptr<FRasterizerState> InRS)
{
	if (InRS != nullptr && CurrentRasterizerState != InRS)
	{
		InRS->Bind(DeviceContext);
		CurrentRasterizerState = InRS;
	}
}

void FRenderStateManager::BindState(std::shared_ptr<FDepthStencilState> InDSS)
{
	if (InDSS != nullptr && CurrentDepthStencilState != InDSS)
	{
		InDSS->Bind(DeviceContext);
		CurrentDepthStencilState = InDSS;
	}
}

void FRenderStateManager::BindState(std::shared_ptr<FBlendState> InBS)
{
	if (InBS != nullptr && CurrentBlendState != InBS)
	{
		InBS->Bind(DeviceContext);
		CurrentBlendState = InBS;
	}
}

void FRenderStateManager::RebindState()
{
	if (CurrentRasterizerState) CurrentRasterizerState->Bind(DeviceContext);
	if (CurrentDepthStencilState) CurrentDepthStencilState->Bind(DeviceContext);
	if (CurrentBlendState) CurrentBlendState->Bind(DeviceContext);
}
