#pragma once

#include "Core/Types/CoreTypes.h"
#include "Editor/UI/Asset/UI/UIEditorDocument.h"

#include <filesystem>

class FUIEditorSerializer
{
public:
	static bool Load(const std::filesystem::path& Path, FUIEditorDocument& OutDocument, FString* OutError = nullptr);
	static bool Save(FUIEditorDocument& Document, FString* OutError = nullptr);

private:
	static bool ParseEditableTextElements(const FString& Rml, FUIEditorDocument& OutDocument, FString* OutError);
};
