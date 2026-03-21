#include "EditorSettings.h"

void FEditorSettings::LoadFromFile(const FString& FilePath)
{
}

void FEditorSettings::SaveToFile(const FString& FilePath) const
{
}

static FEditorSettings& Get()
{
	static FEditorSettings Instance;
	return Instance;
}