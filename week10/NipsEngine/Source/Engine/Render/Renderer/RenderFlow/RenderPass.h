#pragma once
#include "RenderPassContext.h"
#include "Render/Renderer/Renderer.h"

class UMaterial;

class FBaseRenderPass
{
public:
    virtual ~FBaseRenderPass() {}

    virtual bool Initialize() = 0;
    virtual bool Release() = 0;

	/**
	 * Begin, DrawCommand, End 구조를 강제하는 함수
	 * 기본적으로 Override 를 금지하고 있으며, 만약 특수한 패스가 있는 경우 FCustomRenderPass 등의 Class 를 새로 만들어서 virtual 선언할 것 
	 */
    bool Render(const FRenderPassContext* Context);
    ID3D11ShaderResourceView* GetOutSRV() const { return OutSRV; }

protected:
    /** 자원 할당 */
    virtual bool Begin(const FRenderPassContext* Context) = 0;
    /** Draw */
    virtual bool DrawCommand(const FRenderPassContext* Context) = 0;
    /** 자원 해제 */
	virtual bool End(const FRenderPassContext* Context) = 0;

protected:
	// Viewport 출력용 최종 View
    ID3D11ShaderResourceView* OutSRV = nullptr;
};