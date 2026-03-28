#pragma once
#include "Runtime/Viewport.h"
#include "Slate/ISlateViewport.h"

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
	void SetClient(FViewportClient* InClient) { Client = InClient; }
	FViewportClient* GetClient() const { return Client; }

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

private:
	class FViewportClient* Client = nullptr;
};

