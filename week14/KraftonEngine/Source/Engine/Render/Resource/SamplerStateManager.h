#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/Pipeline/RenderConstants.h"

class FSamplerStateManager
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();

	// s0-s2 시스템 샘플러 일괄 바인딩 (프레임 1회)
	void BindSystemSamplers(ID3D11DeviceContext* Ctx);

private:
	ID3D11SamplerState* LinearClampSampler = nullptr;	// s0
	ID3D11SamplerState* LinearWrapSampler = nullptr;	// s1
	ID3D11SamplerState* PointClampSampler = nullptr;	// s2
};
