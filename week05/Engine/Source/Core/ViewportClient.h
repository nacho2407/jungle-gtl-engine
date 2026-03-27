#pragma once

#include "EngineAPI.h"
#include "Windows.h"
#include "Types/String.h"
#include "ShowFlags.h"
#include "Renderer/RenderCommand.h"
#include "Scene/RenderCollector.h"

class FEngineRuntime;
class CRenderer;
class UScene;
class FFrustum;
class UPrimitiveComponent;
struct FRenderCommandQueue;
class UWorld;
class ENGINE_API IViewportClient
{
public:
	virtual ~IViewportClient() = default;

	virtual void Attach(FEngineRuntime* Runtime, CRenderer* Renderer);
	virtual void Detach(FEngineRuntime* Runtime, CRenderer* Renderer);
	virtual void Tick(FEngineRuntime* Runtime, float DeltaTime);
	virtual void HandleMessage(FEngineRuntime* Runtime, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	virtual UScene* ResolveScene(FEngineRuntime* Runtime) const;
	virtual UWorld* ResolveWorld(FEngineRuntime* Runtime) const;
	FShowFlags& GetShowFlags() { return ShowFlags; }
	const FShowFlags& GetShowFlags() const { return ShowFlags; }
	virtual void BuildRenderCommands(FEngineRuntime* Runtime, UScene* Scene,
		const FFrustum& Frustum, FRenderCommandQueue& OutQueue);
	/** 입력 처리는 원래 Viewport 에서 처리하는게 맞는데 구조상 여기다 넣음 */
	virtual void HandleFileDoubleClick(const FString& FilePath);
	virtual void HandleFileDropOnViewport(const FString& FilePath);
protected:
	FShowFlags ShowFlags;
	FSceneRenderCollector RenderCollector;
};

class ENGINE_API CGameViewportClient : public IViewportClient
{
public:
	void Attach(FEngineRuntime* Runtime, CRenderer* Renderer) override;
	void Detach(FEngineRuntime* Runtime, CRenderer* Renderer) override;
};
