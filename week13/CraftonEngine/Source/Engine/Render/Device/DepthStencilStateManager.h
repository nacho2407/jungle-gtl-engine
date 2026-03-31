#pragma once

#include "Render/Types/RenderTypes.h"
#include "Render/Types/RenderStateTypes.h"

class FDepthStencilStateManager
{
public:
	void Create(ID3D11Device* InDevice);
	void Release();
	void Set(ID3D11DeviceContext* InContext, EDepthStencilState InState);
	void ResetCache() { CurrentState = static_cast<EDepthStencilState>(-1); }

private:
	ID3D11DepthStencilState* Default = nullptr;
	ID3D11DepthStencilState* DepthReadOnly = nullptr;
	ID3D11DepthStencilState* StencilWrite = nullptr;
	ID3D11DepthStencilState* StencilMaskEqual = nullptr;
	ID3D11DepthStencilState* NoDepth = nullptr;
	ID3D11DepthStencilState* GizmoInside = nullptr;
	ID3D11DepthStencilState* GizmoOutside = nullptr;

	EDepthStencilState CurrentState = EDepthStencilState::Default;
};
