#pragma once
#include <Core/CoreTypes.h>

struct FSceneView;
struct FViewportMouseEvent;

/**
 * Viewport 의 행동 정책을 정의하는 베이스 인터페이스
 * EditorViewportClient가 이를 상속받음.
 */
class FViewportClient
{
public:
	virtual ~FViewportClient() = default;

	virtual void Tick(float DeltaTime) = 0;
	/**
	 * .
	 * Renderer 에 필요한 SceneView 변수를 매개변수로 받아서 반환해주는 함수
	 * \param OutView 외부에서 인자로 넘겨준 
	 */
	virtual void BuildSceneView(FSceneView& OutView) const = 0;

	/*
	* Mouse Event
	*/
	virtual bool OnMouseMove(const FViewportMouseEvent& Ev) { return false; }
	virtual bool OnMouseButtonDown(const FViewportMouseEvent& Ev) { return false; }
	virtual bool OnMouseButtonUp(const FViewportMouseEvent& Ev) { return false; }
	virtual bool OnMouseWheel(float Delta) { return false; }

	/*
	*  Key Event
	*/
	virtual bool OnKeyDown(uint32 Key) { return false; }
	virtual bool OnKeyUp(uint32 Key) { return false; }
};
