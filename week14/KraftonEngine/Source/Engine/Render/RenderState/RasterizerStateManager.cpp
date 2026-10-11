#include "RasterizerStateManager.h"

#define SAFE_RELEASE(Obj) if (Obj) { Obj->Release(); Obj = nullptr; }

void FRasterizerStateManager::Create(ID3D11Device* InDevice)
{
	D3D11_RASTERIZER_DESC Desc = {};
	Desc.FillMode = D3D11_FILL_SOLID;
	Desc.CullMode = D3D11_CULL_BACK;
	// 반드시 켜야 합니다. Desc를 0으로 초기화하면 DepthClipEnable도 FALSE가 되어
	// shadow camera의 near/far 밖에 있는 caster가 CSM depth map에 새어 들어갈 수 있습니다.
	Desc.DepthClipEnable = TRUE;
	Desc.ScissorEnable = FALSE;
	Desc.MultisampleEnable = FALSE;
	Desc.AntialiasedLineEnable = FALSE;
	InDevice->CreateRasterizerState(&Desc, &BackCull);

	Desc.CullMode = D3D11_CULL_FRONT;
	InDevice->CreateRasterizerState(&Desc, &FrontCull);

	Desc.CullMode = D3D11_CULL_NONE;
	InDevice->CreateRasterizerState(&Desc, &NoCull);

	Desc.FillMode = D3D11_FILL_WIREFRAME;
	Desc.CullMode = D3D11_CULL_NONE;
	InDevice->CreateRasterizerState(&Desc, &WireFrame);

	CurrentState = ERasterizerState::SolidBackCull;
}

void FRasterizerStateManager::Release()
{
	SAFE_RELEASE(BackCull);
	SAFE_RELEASE(FrontCull);
	SAFE_RELEASE(NoCull);
	SAFE_RELEASE(WireFrame);
}

void FRasterizerStateManager::Set(ID3D11DeviceContext* InContext, ERasterizerState InState)
{
	if (CurrentState == InState) return;

	switch (InState)
	{
	case ERasterizerState::SolidBackCull:  InContext->RSSetState(BackCull);  break;
	case ERasterizerState::SolidFrontCull: InContext->RSSetState(FrontCull); break;
	case ERasterizerState::SolidNoCull:    InContext->RSSetState(NoCull);    break;
	case ERasterizerState::WireFrame:      InContext->RSSetState(WireFrame); break;
	}

	CurrentState = InState;
}
