#pragma once

#include "Core/Types/CoreTypes.h"
#include "Editor/Undo/EditorUndoCommand.h"

#include <memory>

class UEditorEngine;

/**
 * @brief 에디터 undo/redo stack 관리자
 */
class FEditorUndoManager
{
public:
	/**
	 * @brief 이미 실행된 명령을 undo stack에 추가합니다.
	 *
	 * @param Command undo stack에 추가할 명령
	 */
	void PushExecutedCommand(std::unique_ptr<IEditorUndoCommand> Command);

	/**
	 * @brief 마지막 명령을 되돌립니다.
	 *
	 * @param EditorEngine 명령을 적용할 에디터 엔진
	 *
	 * @return undo 실행 여부
	 */
	bool Undo(UEditorEngine* EditorEngine);

	/**
	 * @brief 마지막 undo 명령을 다시 실행합니다.
	 *
	 * @param EditorEngine 명령을 적용할 에디터 엔진
	 *
	 * @return redo 실행 여부
	 */
	bool Redo(UEditorEngine* EditorEngine);

	/**
	 * @brief 모든 undo/redo 기록을 비웁니다.
	 */
	void Clear();

	bool CanUndo() const { return !UndoStack.empty(); }
	bool CanRedo() const { return !RedoStack.empty(); }
	bool IsApplying() const { return bIsApplying; }

private:
	void TrimUndoStackToLimit();

	TArray<std::unique_ptr<IEditorUndoCommand>> UndoStack;
	TArray<std::unique_ptr<IEditorUndoCommand>> RedoStack;
	bool bIsApplying = false;
	int32 MaxUndoDepth = 128;
};
