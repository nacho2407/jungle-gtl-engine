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
    void SetClient(const FEditorViewportClient& InClient) { Client = InClient; }
    FEditorViewportClient& GetClient() { return Client; }
    const FEditorViewportClient& GetClient() const { return Client; }

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

	// 단일 Viewport 개선 작업 도중 Device 를 임시로 받게함
	void InitializeResource(ID3D11Device* Device, uint32 Width, uint32 Height);
    bool EnsureResource(ID3D11Device* Device, uint32 Width, uint32 Height);
    void ReleaseResource();

	// 최종 출력 (임시용)
	ID3D11ShaderResourceView* GetOutSRV() const { return ViewportSceneColorSRV.Get(); }

private:
	// Viewport 구조 재편 도중 다형성을 우선 빼고 구현
    FEditorViewportClient Client;
    FEditorViewportState State;

	TComPtr<ID3D11Texture2D> ViewportSceneColorTexture;
    TComPtr<ID3D11RenderTargetView> ViewportSceneColorRTV;
    TComPtr<ID3D11ShaderResourceView> ViewportSceneColorSRV;

    TComPtr<ID3D11Texture2D> ViewportSceneNormalTexture;
    TComPtr<ID3D11RenderTargetView> ViewportSceneNormalRTV;
    TComPtr<ID3D11ShaderResourceView> ViewportSceneNormalSRV;

    TComPtr<ID3D11Texture2D> ViewportSceneLightTexture;
    TComPtr<ID3D11RenderTargetView> ViewportSceneLightRTV;
    TComPtr<ID3D11ShaderResourceView> ViewportSceneLightSRV;

    TComPtr<ID3D11Texture2D> ViewportSceneFogTexture;
    TComPtr<ID3D11RenderTargetView> ViewportSceneFogRTV;
    TComPtr<ID3D11ShaderResourceView> ViewportSceneFogSRV;

    TComPtr<ID3D11Texture2D> ViewportSceneWorldPosTexture;
    TComPtr<ID3D11RenderTargetView> ViewportSceneWorldPosRTV;
    TComPtr<ID3D11ShaderResourceView> ViewportSceneWorldPosSRV;

    TComPtr<ID3D11Texture2D> ViewportSceneFXAATexture;
    TComPtr<ID3D11RenderTargetView> ViewportSceneFXAARTV;
    TComPtr<ID3D11ShaderResourceView> ViewportSceneFXAASRV;

    TComPtr<ID3D11Texture2D> ViewportSelectionMaskTexture;
    TComPtr<ID3D11RenderTargetView> ViewportSelectionMaskRTV;
    TComPtr<ID3D11ShaderResourceView> ViewportSelectionMaskSRV;

    TComPtr<ID3D11Texture2D> DepthStencilBuffer;
    TComPtr<ID3D11DepthStencilView> DepthStencilView;
    TComPtr<ID3D11Texture2D> ViewportDepthStencilTexture;
    TComPtr<ID3D11DepthStencilView> ViewportDepthStencilView;
    TComPtr<ID3D11ShaderResourceView> ViewportDepthStencilSRV;

	uint32 ViewportRenderTargetWidth = 0;
    uint32 ViewportRenderTargetHeight = 0;
};

