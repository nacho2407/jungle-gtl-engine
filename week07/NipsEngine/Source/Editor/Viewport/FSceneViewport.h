#pragma once
#include "Runtime/Viewport.h"
#include "Slate/ISlateViewport.h"
#include "Editor/EditorUtils.h"
#include "Editor/Viewport/EditorViewportClient.h"
#include "Render/Device/D3DDevice.h"  // FRenderTargetSet 때문에 포함했는데 따로 분리 필요할듯

class FViewportClient;
struct FViewportMouseEvent;
    /*
* 실제 viewport 입력/출력 창구
* FViewportClient 로 이벤트 전달
* viewport local rect 를 알고 있음
* ViewportClient <- > Viewport 상호 참조 가능(소유권은 상위 관리자가 보유)
*/

class FSceneViewport : public FViewport, public ISlateViewport
{
public:
    void SetClient(FEditorViewportClient* InClient) { Client = InClient; }
    FEditorViewportClient* GetClient() { return Client; }
    const FEditorViewportClient* GetClient() const { return Client; }

	/*
	* ISlateViewport Interface
	*/
	void Draw() override;

	bool ContainsPoint(int32 X, int32 Y) const override;
	void WindowToLocal(int32 X, int32 Y, int32& OutX, int32& OutY) const override;

	bool OnMouseMove(const FViewportMouseEvent& Ev) override;
	bool OnMouseButtonDown(const FViewportMouseEvent& Ev) override;
	bool OnMouseButtonUp(const FViewportMouseEvent& Ev) override;
	bool OnMouseWheel(float Delta) override;

	bool OnKeyDown(uint32 Key) override;
	bool OnKeyUp(uint32 Key) override;
	bool OnChar(uint32 Codepoint) override;


	void SetRect(const FViewportRect& InRect) override
	{
		Rect = InRect;
	}
	const FViewportRect& GetRect() const override
	{
		return Rect;
	}

	FEditorViewportState& GetState() { return State; }
    const FEditorViewportState& GetState() const { return State; }
    void SetState(const FEditorViewportState& InState) { State = InState; }

	FRenderTargetSet GetViewportRenderTargets() const;

	// 최종 출력 (임시용)
	ID3D11ShaderResourceView* GetOutSRV() const 
	{ 
		if (!RenderTargetSet)
            return nullptr;
		return RenderTargetSet->SceneColorSRV;
	}

	void SetRenderTargetSet(FRenderTargetSet* InRenderTargetSet) { RenderTargetSet = InRenderTargetSet; }
    FRenderTargetSet* GetRenderTargetSet() const { return RenderTargetSet; }

private:
	// FViewport 내에서 FViewportClient 로 추상화하는 것이 맞지만, 현재로썬 다형성을 제대로 활용하지 않는 상태라 임시로 다음과 같이 구성
    FEditorViewportClient* Client = nullptr;
    FEditorViewportState State;

	// Renderer 의 자원을 참조
	FRenderTargetSet* RenderTargetSet = nullptr;

	uint32 ViewportRenderTargetWidth = 0;
    uint32 ViewportRenderTargetHeight = 0;
};

