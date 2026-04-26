#pragma once

#include "Render/RenderPass/PassRenderStateTable.h"

class FD3DDevice;
class FDrawCommandList;
class FRenderer;
class FScene;
struct FFrameContext;
struct FStateCache;
struct FSystemResources;

/*
	FPassContext — 렌더패스에 전달되는 컨텍스트 번들.
	Device, Frame, Cache, Resources, CommandList, Renderer, Scene 참조를 한 번에 전달합니다.
*/
struct FPassContext
{
	FD3DDevice&          Device;
	const FFrameContext&  Frame;
	FStateCache&         Cache;
	FSystemResources&    Resources;
	FDrawCommandList&    CommandList;
	FRenderer*           Renderer;
	FScene*              Scene = nullptr;
};

/*
	FRenderPassBase — ERenderPass enum 1:1 대응 OOP 렌더패스 기본 클래스.
	각 패스는 고유 렌더 상태(FPassRenderState)를 소유하며,
	패스 전후 GPU 상태 전환 로직을 BeginPass/EndPass로 캡슐화합니다.

	기존 FPassRenderStateTable의 상태 정의 + FPassEventBuilder의 이벤트 로직을
	패스 클래스 단위로 응집시켜, 새 패스 추가 시 클래스 하나만 작성하면 됩니다.
*/
class FRenderPassBase
{
public:
	virtual ~FRenderPassBase() = default;

	ERenderPass             GetPassType()    const { return PassType; }
	const FPassRenderState& GetRenderState() const { return RenderState; }

	// 패스 전후 GPU 상태 전환 (RT 바인딩, 리소스 복사 등)
	virtual bool BeginPass(const FPassContext& Ctx) { return true; }
	virtual void EndPass(const FPassContext& Ctx) {}

	// 패스 실행 — 기본 구현: DrawCommandList에서 패스 범위를 가져와 Submit.
	// ShadowDepth(Cubemap 6면, CSM cascade) 등 커스텀 렌더링이 필요한 패스는 override.
	virtual void Execute(const FPassContext& Ctx);

protected:
	ERenderPass      PassType = ERenderPass::Opaque;
	FPassRenderState RenderState;
};
