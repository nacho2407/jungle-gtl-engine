#pragma once

#include "Core/CoreTypes.h"
#include "Editor/UI/Asset/AssetEditorWidget.h"

#include <memory>

class UObject;
class IEditorPreviewViewportClient;

class FAssetEditorManager
{
public:
	~FAssetEditorManager();

	template<typename TEditor, typename... TArgs>
	TEditor& RegisterEditor(TArgs&&... Args)
	{
		auto Editor = std::make_unique<TEditor>(std::forward<TArgs>(Args)...);
		TEditor& Ref = *Editor;
		Editors.push_back(std::move(Editor));
		return Ref;
	}

	void Tick(float DeltaTime);
	void Render(float DeltaTime);

	bool OpenEditorForObject(UObject* Object);

	void CollectPreviewViewportClients(TArray<IEditorPreviewViewportClient*>& OutClients) const;

	bool IsMouseOverAnyEditorViewport() const;

private:
	TArray<std::unique_ptr<FAssetEditorWidget>> Editors;
};
