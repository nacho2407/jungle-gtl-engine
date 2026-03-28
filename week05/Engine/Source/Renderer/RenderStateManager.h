#pragma once

#include "RenderState.h"
#include "Types/CoreTypes.h"
#include "Types/Map.h"
#include <wrl/client.h>
#include <memory>

using Microsoft::WRL::ComPtr;

/// RasterizerState, BlendState, DepthStencilState 등의 Renderer state 담당
/// 현재 RasterizerState만 적용된 상태
class ENGINE_API FRenderStateManager
{
private:
	ID3D11Device* Device;
	ID3D11DeviceContext* DeviceContext;
	TMap<uint32, std::shared_ptr<FRasterizerState>> RasterizerStateMap;
	TMap<uint32, std::shared_ptr<FDepthStencilState>> DepthStencilStateMap;
	TMap<uint32, std::shared_ptr<FBlendState>> BlendStateMap;

	std::shared_ptr<FRasterizerState> CurrentRasterizerState = nullptr;
	std::shared_ptr<FDepthStencilState> CurrentDepthStencilState = nullptr;
	std::shared_ptr<FBlendState> CurrentBlendState = nullptr;

public:
	FRenderStateManager(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext)
		: Device(InDevice)
		, DeviceContext(InDeviceContext)
	{}

	// 자주 사용되는 상태들을 미리 생성
	void PrepareCommonStates();

	// 옵션에 따른 상태 반환 (없으면 생성)
	std::shared_ptr<FRasterizerState> GetOrCreateRasterizerState(const FRasterizerStateOption& opt);
	std::shared_ptr<FDepthStencilState> GetOrCreateDepthStencilState(const FDepthStencilStateOption& opt);
	std::shared_ptr<FBlendState> GetOrCreateBlendState(const FBlendStateOption& opt);

	// 실제 상태 적용
	void BindState(std::shared_ptr<FRasterizerState> InRS);
	void BindState(std::shared_ptr<FDepthStencilState> InDSS);
	void BindState(std::shared_ptr<FBlendState> InBS);
	void RebindState();
};
