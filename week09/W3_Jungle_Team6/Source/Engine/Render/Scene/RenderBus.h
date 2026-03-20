#pragma once

/*
	
	는 Renderer에게 Draw Call 요청을 vector의 형태로 전달하는 역할을 합니다.
	Renderer가 RenderBus에 담긴 Draw Call 요청들을 처리할 수 있게 합니다.
*/

//	TODO : CoreType.h 경로 변경 요구
#include "Core/CoreTypes.h"
#include "Render/Scene/RenderCommand.h"

struct FRenderHandler
{
	bool bGridVisible = true;
};

class FRenderBus
{
private:
	TArray<FRenderCommand> PassQueues[(uint32)ERenderPass::MAX];

public:
	void Clear();
	void AddCommand(ERenderPass Pass, const FRenderCommand& InCommand);
	const TArray<FRenderCommand>& GetCommands(ERenderPass Pass) const;
};

