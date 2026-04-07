#pragma once

#include "EngineAPI.h"
#include "Windows.h"
#include "Types/String.h"
#include "ShowFlags.h"
#include "Renderer/RenderCommand.h"
#include "Level/RenderCollector.h"

class FEngine;
class FRenderer;
class ULevel;
class FFrustum;
class UPrimitiveComponent;
struct FRenderCommandQueue;
class UWorld;

class ENGINE_API IViewportClient
{
public:
	virtual ~IViewportClient() = default;

	/** 현재 엔진과 렌더러에 자신을 연결할 때 한 번 호출된다. 리소스 바인딩 시작 지점이다. */
	virtual void Attach(FEngine* Engine, FRenderer* Renderer);
	/** 다른 뷰포트로 교체되거나 종료될 때 호출된다. Attach에서 잡은 상태를 정리한다. */
	virtual void Detach(FEngine* Engine, FRenderer* Renderer);
	/** 입력 누적값이나 카메라 상태처럼 뷰포트 전용 로직을 프레임마다 갱신한다. */
	virtual void Tick(FEngine* Engine, float DeltaTime);
	/** 윈도우 메시지를 직접 다루고 싶을 때 사용하는 후킹 지점이다. */
	virtual void HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	/** 이 뷰포트가 어떤 씬을 바라볼지 결정한다. 기본값은 엔진의 활성 씬이다. */
	virtual ULevel* ResolveScene(FEngine* Engine) const;
	/** 이 뷰포트가 어떤 월드를 조작할지 결정한다. 기본값은 엔진의 활성 월드다. */
	virtual UWorld* ResolveWorld(FEngine* Engine) const;
	/** 프러스텀 컬링과 수집기를 이용해 실제 렌더 큐에 커맨드를 채운다. */
	virtual void BuildRenderCommands(FEngine* Engine, ULevel* Scene,
		const FFrustum& Frustum, const FShowFlags& Flags, const FVector& CameraPosition, FRenderCommandQueue& OutQueue);
	/** 에셋 브라우저와의 상호작용을 위해 파일 더블클릭을 뷰포트가 직접 처리할 수 있게 열어둔 훅이다. */
	virtual void HandleFileDoubleClick(const FString& FilePath);
	/** 파일을 뷰포트 위로 드롭했을 때 배치/가져오기 로직을 구현하는 훅이다. */
	virtual void HandleFileDropOnViewport(const FString& FilePath);
	/** 필요 시 뷰포트가 자체 렌더 루프를 수행하도록 남겨둔 진입점이다. */
	virtual void Render(FEngine* Engine, FRenderer* Renderer);

protected:
	/** 액터 목록을 순회하며 실제 드로우 커맨드로 바꾸는 공용 수집기다. */
	FSceneRenderCollector RenderCollector;
};

class ENGINE_API FGameViewportClient : public IViewportClient
{
public:
	/** 게임 뷰포트에 필요한 렌더러 콜백을 연결한다. */
	void Attach(FEngine* Engine, FRenderer* Renderer) override;
	/** 게임 뷰포트가 사용하던 렌더러 콜백을 해제한다. */
	void Detach(FEngine* Engine, FRenderer* Renderer) override;
	/** 활성 게임 월드를 기준으로 기본 렌더 패스를 실행한다. */
	void Render(FEngine* Engine, FRenderer* Renderer) override;
};
