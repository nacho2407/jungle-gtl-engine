#pragma once

#include "Render/Types/POVProvider.h"
#include "Render/Types/ViewTypes.h"

class FViewport;
class UWorld;

class IEditorPreviewViewportClient : public IPOVProvider
{
public:
	virtual ~IEditorPreviewViewportClient() = default;

	virtual bool IsRenderable() const = 0;
	virtual bool IsMouseOverViewport() const = 0;

	virtual FViewport* GetViewport() const = 0;
	virtual UWorld* GetPreviewWorld() const = 0;

	virtual FViewportRenderOptions& GetRenderOptions() = 0;
	virtual const FViewportRenderOptions& GetRenderOptions() const = 0;

	virtual void NotifyViewportResized(int32 NewWidth, int32 NewHeight) = 0;

	// 렌더 단계(WorldTick 이후)에서 디버그 라인을 큐에 제출하는 훅.
	// DrawDebug 라인은 1프레임 수명이라 WorldTick의 queue.Tick()에 지워진다.
	// 따라서 위젯 Tick이 아니라 이 시점에 추가해야 CollectDebugDraw가 같은 프레임에 수집한다.
	virtual void SubmitFrameDebugDraw() {}
};
