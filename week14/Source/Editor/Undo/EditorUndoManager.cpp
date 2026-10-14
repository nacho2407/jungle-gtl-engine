#include "Editor/Undo/EditorUndoManager.h"

#include "Editor/EditorEngine.h"

void FEditorUndoManager::PushExecutedCommand(std::unique_ptr<IEditorUndoCommand> Command)
{
	if (!Command || bIsApplying)
	{
		return;
	}

	// 새 편집 명령이 들어오면 기존 redo 경로는 더 이상 현재 scene 상태와 맞지 않습니다.
	RedoStack.clear();
	UndoStack.push_back(std::move(Command));
	TrimUndoStackToLimit();
}

bool FEditorUndoManager::Undo(UEditorEngine* EditorEngine)
{
	if (!EditorEngine || UndoStack.empty() || bIsApplying)
	{
		return false;
	}

	std::unique_ptr<IEditorUndoCommand> Command = std::move(UndoStack.back());
	UndoStack.pop_back();

	bIsApplying = true;
	Command->Undo(EditorEngine);
	bIsApplying = false;

	RedoStack.push_back(std::move(Command));
	return true;
}

bool FEditorUndoManager::Redo(UEditorEngine* EditorEngine)
{
	if (!EditorEngine || RedoStack.empty() || bIsApplying)
	{
		return false;
	}

	std::unique_ptr<IEditorUndoCommand> Command = std::move(RedoStack.back());
	RedoStack.pop_back();

	bIsApplying = true;
	Command->Redo(EditorEngine);
	bIsApplying = false;

	UndoStack.push_back(std::move(Command));
	TrimUndoStackToLimit();
	return true;
}

void FEditorUndoManager::Clear()
{
	UndoStack.clear();
	RedoStack.clear();
	bIsApplying = false;
}

void FEditorUndoManager::TrimUndoStackToLimit()
{
	while (static_cast<int32>(UndoStack.size()) > MaxUndoDepth)
	{
		UndoStack.erase(UndoStack.begin());
	}
}
