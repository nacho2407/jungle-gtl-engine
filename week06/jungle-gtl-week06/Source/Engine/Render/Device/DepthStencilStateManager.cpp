#include "DepthStencilStateManager.h"

#define SAFE_RELEASE(Obj) if (Obj) { Obj->Release(); Obj = nullptr; }

void FDepthStencilStateManager::Create(ID3D11Device* InDevice)
{
	// Default
	D3D11_DEPTH_STENCIL_DESC Desc = {};
	Desc.DepthEnable = TRUE;
	Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	Desc.DepthFunc = D3D11_COMPARISON_LESS;
	Desc.StencilEnable = FALSE;
	InDevice->CreateDepthStencilState(&Desc, &Default);

	// Depth Read Only
	Desc = {};
	Desc.DepthEnable = TRUE;
	Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	Desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	Desc.StencilEnable = FALSE;
	InDevice->CreateDepthStencilState(&Desc, &DepthReadOnly);

	// Stencil Write
	Desc = {};
	Desc.DepthEnable = TRUE;
	Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	Desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	Desc.StencilEnable = TRUE;
	Desc.StencilReadMask = 0xFF;
	Desc.StencilWriteMask = 0xFF;
	Desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
	Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
	Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	Desc.BackFace = Desc.FrontFace;
	InDevice->CreateDepthStencilState(&Desc, &StencilWrite);

	// No Depth
	Desc = {};
	Desc.DepthEnable = FALSE;
	Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	Desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	Desc.StencilEnable = FALSE;
	InDevice->CreateDepthStencilState(&Desc, &NoDepth);

	// Gizmo Inside (Stencil Equal, read-only)
	Desc = {};
	Desc.DepthEnable = TRUE;
	Desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	Desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
	Desc.StencilEnable = TRUE;
	Desc.StencilReadMask = 0xFF;
	Desc.StencilWriteMask = 0x00;
	Desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	Desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	Desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	Desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	Desc.BackFace = Desc.FrontFace;
	InDevice->CreateDepthStencilState(&Desc, &GizmoInside);

	// Gizmo Outside (Stencil Not Equal, read-only)
	Desc.FrontFace.StencilFunc = D3D11_COMPARISON_NOT_EQUAL;
	Desc.BackFace = Desc.FrontFace;
	InDevice->CreateDepthStencilState(&Desc, &GizmoOutside);

	// Stencil Mask Equal (read-only)
	Desc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
	Desc.BackFace = Desc.FrontFace;
	InDevice->CreateDepthStencilState(&Desc, &StencilMaskEqual);

}

void FDepthStencilStateManager::Release()
{
	SAFE_RELEASE(Default);
	SAFE_RELEASE(DepthReadOnly);
	SAFE_RELEASE(StencilWrite);
	SAFE_RELEASE(StencilMaskEqual);
	SAFE_RELEASE(NoDepth);
	SAFE_RELEASE(GizmoInside);
	SAFE_RELEASE(GizmoOutside);
}

void FDepthStencilStateManager::Set(ID3D11DeviceContext* InContext, EDepthStencilState InState)
{
	if (CurrentState == InState) return;

	switch (InState)
	{
	case EDepthStencilState::Default:              InContext->OMSetDepthStencilState(Default, 0);          break;
	case EDepthStencilState::DepthReadOnly:        InContext->OMSetDepthStencilState(DepthReadOnly, 0);    break;
	case EDepthStencilState::StencilWrite:         InContext->OMSetDepthStencilState(StencilWrite, 1);     break;
	case EDepthStencilState::StencilWriteOnlyEqual:InContext->OMSetDepthStencilState(StencilMaskEqual, 1); break;
	case EDepthStencilState::NoDepth:              InContext->OMSetDepthStencilState(NoDepth, 0);          break;
	case EDepthStencilState::GizmoInside:          InContext->OMSetDepthStencilState(GizmoInside, 1);      break;
	case EDepthStencilState::GizmoOutside:         InContext->OMSetDepthStencilState(GizmoOutside, 1);     break;
	}

	CurrentState = InState;
}
