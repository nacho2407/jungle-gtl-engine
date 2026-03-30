#pragma once

#include "EngineAPI.h"
#include "Windows.h"
#include "Types/String.h"
#include "ShowFlags.h"
#include "Renderer/RenderCommand.h"
#include "Scene/RenderCollector.h"

class FEngine;
class FRenderer;
class UScene;
class FFrustum;
class UPrimitiveComponent;
struct FRenderCommandQueue;
class UWorld;
class ENGINE_API IViewportClient
{
public:
	virtual ~IViewportClient() = default;

	virtual void Attach(FEngine* Engine, FRenderer* Renderer);
	virtual void Detach(FEngine* Engine, FRenderer* Renderer);
	virtual void Tick(FEngine* Engine, float DeltaTime);
	virtual void HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	virtual UScene* ResolveScene(FEngine* Engine) const;
	virtual UWorld* ResolveWorld(FEngine* Engine) const;
	virtual void BuildRenderCommands(FEngine* Engine, UScene* Scene,
		const FFrustum& Frustum, const FShowFlags& Flags, const FVector& CameraPosition, FRenderCommandQueue& OutQueue);
	/** 입력 처리는 원래 Viewport 에서 처리하는게 맞는데 구조상 여기다 넣음 */
	virtual void HandleFileDoubleClick(const FString& FilePath);
	virtual void HandleFileDropOnViewport(const FString& FilePath);
	virtual void Render(FEngine* Engine, FRenderer* Renderer);
protected:
	FSceneRenderCollector RenderCollector;
};

class ENGINE_API FGameViewportClient : public IViewportClient
{
public:
	void Attach(FEngine* Engine, FRenderer* Renderer) override;
	void Detach(FEngine* Engine, FRenderer* Renderer) override;
	void Render(FEngine* Engine, FRenderer* Renderer) override;
};
