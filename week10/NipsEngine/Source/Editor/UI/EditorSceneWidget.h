#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Core/CoreMinimal.h"

class AActor;

class FEditorSceneWidget : public FEditorWidget
{
public:
	virtual void Initialize(UEditorEngine* InEditorEngine) override;
	virtual void Render(float DeltaTime) override;

private:
	bool PromptSavePrefabAs(const AActor* Actor, FString& OutFilePath) const;

private:
	int32 LastClickedActorIndex = -1;
	bool bOpenOutlinerContextMenu = false;
	bool bOpenRenameActorPopup = false;
	AActor* PendingRenameActor = nullptr;
	char OutlinerSearchText[128] = "";
	char RenameActorName[128] = "";
};
