#include "Editor/Command/EditorCommandSystem.h"

#include "Editor/EditorEngine.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

void FEditorCommandSystem::Initialize(UEditorEngine* InEditorEngine)
{
	EditorEngine = InEditorEngine;
}

bool FEditorCommandSystem::CanExecute(EEditorCommand Command, const FEditorCommandArgs& Args) const
{
	if (!EditorEngine)
	{
		return false;
	}

	switch (Command)
	{
	case EEditorCommand::OpenScene:
	case EEditorCommand::SaveSceneAs:
		return !Args.ScenePath.empty();
	case EEditorCommand::Undo:
		return !EditorEngine->GetUndoSystem().GetUndoHistory().empty();
	case EEditorCommand::Redo:
		return !EditorEngine->GetUndoSystem().GetRedoHistory().empty();
	case EEditorCommand::ClearUndoHistory:
		return !EditorEngine->GetUndoSystem().GetUndoHistory().empty() ||
			!EditorEngine->GetUndoSystem().GetRedoHistory().empty();
	case EEditorCommand::RestoreUndoHistoryIndex:
		return Args.HistoryIndex >= 0 &&
			Args.HistoryIndex < static_cast<int32>(EditorEngine->GetUndoSystem().GetUndoHistory().size());
	case EEditorCommand::NewScene:
	case EEditorCommand::SaveScene:
	case EEditorCommand::RefreshAssets:
	case EEditorCommand::StartPlay:
	case EEditorCommand::StopPlay:
	default:
		return true;
	}
}

bool FEditorCommandSystem::Execute(EEditorCommand Command, const FEditorCommandArgs& Args)
{
	if (!CanExecute(Command, Args))
	{
		return false;
	}

	switch (Command)
	{
	case EEditorCommand::NewScene:
		return EditorEngine->GetSceneService().NewScene().bSuccess;
	case EEditorCommand::OpenScene:
		return EditorEngine->GetSceneService().OpenScene(Args.ScenePath, Args.bPromptSave).bSuccess;
	case EEditorCommand::SaveScene:
		return EditorEngine->GetSceneService().SaveScene().bSuccess;
	case EEditorCommand::SaveSceneAs:
		return EditorEngine->GetSceneService().SaveSceneToFilePath(Args.ScenePath).bSuccess;
	case EEditorCommand::Undo:
		return EditorEngine->GetUndoSystem().Undo();
	case EEditorCommand::Redo:
		return EditorEngine->GetUndoSystem().Redo();
	case EEditorCommand::ClearUndoHistory:
		EditorEngine->GetUndoSystem().ClearHistory();
		return true;
	case EEditorCommand::RestoreUndoHistoryIndex:
		return EditorEngine->GetUndoSystem().RestoreHistoryIndex(Args.HistoryIndex);
	case EEditorCommand::RefreshAssets:
		FResourceManager::Get().RefreshFromAssetDirectory(FPaths::ToUtf8(FPaths::AssetDirectoryPath()));
		EditorEngine->GetAssetService().RefreshAssetDatabase();
		EditorEngine->GetNotificationService().Info("Assets reloaded");
		return true;
	case EEditorCommand::StartPlay:
		EditorEngine->StartPlaySession();
		return true;
	case EEditorCommand::StopPlay:
		EditorEngine->StopPlaySession();
		return true;
	default:
		return false;
	}
}
