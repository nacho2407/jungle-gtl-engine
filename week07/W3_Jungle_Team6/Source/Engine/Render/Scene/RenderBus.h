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

public:

private:
	TArray<FRenderCommand> ComponentCommands;
	TArray<FRenderCommand> DepthLessCommands;
	TArray<FRenderCommand> EditorCommands;
	TArray<FRenderCommand> EditorGridCommands;
	
	//	Array로 하지 않아도 되지만, 추후를 위한 확장성을 고려하여 추가하였습니다.
	TArray<FRenderCommand> OutlineCommands;
	TArray<FRenderCommand> OverlayCommands;

public:
	void Clear();
	//	Draw Call 요청을 RenderBus에 추가하는 함수
	void AddComponentCommand(const FRenderCommand& InCommand);
	void AddDepthLessCommand(const FRenderCommand& InCommand);
	void AddEditorCommand(const FRenderCommand& InCommand);
	void AddGridEditorCommand(const FRenderCommand& InCommand);
	void AddOutlineCommand(const FRenderCommand& InCommand);
	void AddOverlayCommand(const FRenderCommand& InCommand);

	const TArray<FRenderCommand>& GetComponentCommands() const { return ComponentCommands; }
	const TArray<FRenderCommand>& GetDepthLessCommands() const { return DepthLessCommands; }
	const TArray<FRenderCommand>& GetEditorCommand() const { return EditorCommands; }
	const TArray<FRenderCommand>& GetGridEditorCommand() const { return EditorGridCommands; }
	const TArray<FRenderCommand>& GetSelectionOutlineCommands() const { return OutlineCommands; }
	const TArray<FRenderCommand>& GetOverlayCommands() const { return OverlayCommands; }
};

