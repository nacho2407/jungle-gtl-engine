#pragma once

#include "CoreMinimal.h"
#include "Core/ShowFlags.h"
#include "Level/SceneRenderPacket.h"

class UActorComponent;
class AActor;
class FFrustum;
class UPrimitiveComponent;

/**
 * 월드의 프리미티브를 뷰 단위 씬 패킷으로 수집하는 프런트엔드다.
 * 여기서는 가시성 판단과 분류만 수행하고, GPU 제출이나 렌더러 내부 기능 호출은 하지 않는다.
 */
class ENGINE_API FScenePacketBuilder
{
public:
	// 액터와 컴포넌트를 순회하며 보이는 프리미티브를 씬 패킷에 기록한다.
	void BuildScenePacket(
		const TArray<AActor*>& Actors,
		const FFrustum& Frustum,
		const FShowFlags& ShowFlags,
		FSceneRenderPacket& OutPacket);

	// 주어진 프러스텀과 ShowFlag 기준으로 보이는 프리미티브 집합을 만든다.
	void FrustumCull(
		const TArray<AActor*>& Actors,
		const FFrustum& Frustum,
		const FShowFlags& ShowFlags,
		TArray<UPrimitiveComponent*>& OutVisible);
};
