#include "RenderBus.h"

#if DEBUG

#include <iostream>

#endif

void FRenderBus::Clear()
{
	ComponentCommands.clear();
	DepthLessCommands.clear();
	EditorCommands.clear();
	EditorGridCommands.clear();
	OverlayCommands.clear();
	OutlineCommands.clear();
}

void FRenderBus::AddComponentCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	ComponentCommands.push_back(InCommand);
}

void FRenderBus::AddDepthLessCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	DepthLessCommands.push_back(InCommand);
}

void FRenderBus::AddEditorCommand(const FRenderCommand& InCommand)
{
	if(InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	EditorCommands.push_back(InCommand);
}

void FRenderBus::AddGridEditorCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	EditorGridCommands.push_back(InCommand);
}

void FRenderBus::AddOutlineCommand(const FRenderCommand& InCommand)
{
	if(InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	OutlineCommands.push_back(InCommand);
}

void FRenderBus::AddOverlayCommand(const FRenderCommand& InCommand)
{
	if (InCommand.MeshBuffer == nullptr)
	{
		return;
	}
	OverlayCommands.push_back(InCommand);
}

