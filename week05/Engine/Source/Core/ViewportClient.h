#pragma once

#include "EngineAPI.h"
#include "Windows.h"
#include "Types/String.h"
#include "ShowFlags.h"
#include "Level/ScenePacketBuilder.h"
#include "Level/SceneRenderPacket.h"

class FEngine;
class FRenderer;
class ULevel;
class FFrustum;
class UPrimitiveComponent;
class UWorld;

/**
 * 엔진과 렌더러 사이에서 뷰 단위 프레임 데이터를 준비하는 인터페이스다.
 * 게임, 에디터, 프리뷰 등 각 모드별 뷰포트는 이 인터페이스를 통해
 * 입력 처리, 월드 해석, 씬 패킷 생성, 렌더 요청 구성을 담당한다.
 */
class ENGINE_API IViewportClient
{
public:
	virtual ~IViewportClient() = default;

	// 엔진이 이 뷰포트 클라이언트를 활성화할 때 호출한다.
	virtual void Attach(FEngine* Engine, FRenderer* Renderer);
	// 엔진이 이 뷰포트 클라이언트를 비활성화할 때 호출한다.
	virtual void Detach(FEngine* Engine, FRenderer* Renderer);
	// 프레임마다 뷰포트 로직과 입력 반응을 갱신한다.
	virtual void Tick(FEngine* Engine, float DeltaTime);
	// 윈도우 메시지를 뷰포트 단위로 처리할 기회를 제공한다.
	virtual void HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam);
	// 현재 뷰포트가 렌더해야 할 레벨을 결정한다.
	virtual ULevel* ResolveScene(FEngine* Engine) const;
	// 현재 뷰포트가 참조해야 할 월드를 결정한다.
	virtual UWorld* ResolveWorld(FEngine* Engine) const;
	// 월드와 프러스텀을 바탕으로 렌더러 비의존적인 씬 패킷을 만든다.
	virtual void BuildSceneRenderPacket(
		FEngine* Engine,
		UWorld* World,
		const FFrustum& Frustum,
		const FShowFlags& Flags,
		FSceneRenderPacket& OutPacket);
	// 콘텐츠 브라우저 등의 더블클릭 동작을 뷰포트가 처리할 수 있게 한다.
	virtual void HandleFileDoubleClick(const FString& FilePath);
	// 파일 드롭 이벤트를 뷰포트가 처리할 수 있게 한다.
	virtual void HandleFileDropOnViewport(const FString& FilePath);
	// 한 프레임에 필요한 렌더 요청을 만들어 렌더러에 전달한다.
	virtual void Render(FEngine* Engine, FRenderer* Renderer);

protected:
	// 월드 프리미티브를 씬 패킷으로 바꾸는 프런트엔드다.
	FScenePacketBuilder ScenePacketBuilder;
};

/**
 * 런타임 게임 화면용 기본 뷰포트 클라이언트다.
 * 활성 카메라를 기준으로 씬 패킷과 게임 프레임 요청을 구성한다.
 */
class ENGINE_API FGameViewportClient : public IViewportClient
{
public:
	// 게임 뷰포트가 활성화될 때 필요한 상태를 연결한다.
	void Attach(FEngine* Engine, FRenderer* Renderer) override;
	// 게임 뷰포트가 비활성화될 때 상태를 정리한다.
	void Detach(FEngine* Engine, FRenderer* Renderer) override;
	// 현재 게임 월드를 기준으로 한 프레임 렌더 요청을 만든다.
	void Render(FEngine* Engine, FRenderer* Renderer) override;
};
