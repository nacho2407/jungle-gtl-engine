#pragma once

#include "Core/Types/CoreTypes.h"
#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Object/GarbageCollection.h"

#include <functional>
#include <memory>

class UObject;
class IEditorPreviewViewportClient;
class UEditorEngine;

class FAssetEditorManager : public FGCObject
{
public:
	~FAssetEditorManager();
	void Initialize(UEditorEngine* InEditorEngine) { EditorEngine = InEditorEngine; }

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

	void CloseAll();
	bool OpenEditorForObject(UObject* Object);

	void CollectPreviewViewportClients(TArray<IEditorPreviewViewportClient*>& OutClients) const;

	bool IsMouseOverAnyEditorViewport() const;

	const char* GetReferencerName() const override { return "FAssetEditorManager"; }
	void AddReferencedObjects(FReferenceCollector& Collector) override;

	void RemoveClosedEditors();

private:
	UEditorEngine* EditorEngine = nullptr;
	TArray<std::function<std::unique_ptr<FAssetEditorWidget>()>> EditorFactories;
	TArray<std::unique_ptr<FAssetEditorWidget>> OpenEditors;
};
