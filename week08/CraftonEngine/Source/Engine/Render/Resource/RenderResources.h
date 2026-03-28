#pragma once
#include "Render/Resource/Buffer.h"
#include "Render/Pipeline/RenderCommand.h"

/*
	공용 Constant Buffer를 관리하는 구조체입니다.
	모든 커맨드가 공통으로 사용하는 Frame/PerObject CB만 소유합니다.
	타입별 CB(Gizmo, Editor, Outline 등)는 FConstantBufferPool에서 관리됩니다.
*/

struct FRenderResources
{
	FConstantBuffer FrameBuffer;				// b0 — ECBSlot::Frame
	FConstantBuffer PerObjectConstantBuffer;	// b1 — ECBSlot::PerObject

	void Create(ID3D11Device* InDevice);
	void Release();
};
