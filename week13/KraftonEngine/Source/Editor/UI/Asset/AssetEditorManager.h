#pragma once

#include "Core/CoreTypes.h"
#include "Editor/UI/Asset/AssetEditorWidget.h"

#include <functional>
#include <memory>

class UObject;
class IEditorPreviewViewportClient;

class FAssetEditorManager
{
public:
	~FAssetEditorManager();

	template<typename TEditor, typename... TArgs>
	void RegisterEditor(TArgs&&... Args)
	{
		EditorFactories.push_back([Args...]()
		{
			return std::make_unique<TEditor>(Args...);
		});
	}

	void Tick(float DeltaTime);
	void Render(float DeltaTime);

	bool OpenEditorForObject(UObject* Object);

	void CollectPreviewViewportClients(TArray<IEditorPreviewViewportClient*>& OutClients) const;

	bool IsMouseOverAnyEditorViewport() const;

	void RemoveClosedEditors();

private:
	TArray<std::function<std::unique_ptr<FAssetEditorWidget>()>> EditorFactories;
	TArray<std::unique_ptr<FAssetEditorWidget>> OpenEditors;
};
