#pragma once

#include "Core/Engine.h"

class ENGINE_API FGameEngine : public FEngine
{
public:
	FGameEngine() = default;
	~FGameEngine() override = default;

protected:
	/** 게임 실행에 필요한 월드 컨텍스트를 생성하고 기본 씬을 로드한다. */
	bool InitializeWorlds() override;
	/** 게임 모드에서 사용할 기본 뷰포트 클라이언트를 만든다. */
	std::unique_ptr<IViewportClient> CreateViewportClient() override;
	/** 매 프레임 게임 월드를 진행시킨다. */
	void TickWorlds(float DeltaTime) override;
};