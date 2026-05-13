#pragma once
#include "UI/EditorWidget.h"

class UObject;
class IEditorPreviewViewportClient;

class FAssetEditorWidget : public FEditorWidget
{
public:
	virtual ~FAssetEditorWidget() = default;

	virtual bool CanEdit(UObject* Object) const = 0;

	virtual void Open(UObject* Object);
	virtual void Close();
	virtual void Tick(float DeltaTime) {}

	virtual void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const {}

	virtual bool AllowsMultipleInstances() const { return false; }

	UObject* GetEditedObject() const { return EditedObject; }
	bool IsOpen() const { return bOpen; }

protected:
	void MarkDirty() { bDirty = true; }
	void ClearDirty() { bDirty = false; }
	bool IsDirty() const { return bDirty; }

protected:
	UObject* EditedObject = nullptr;
	bool bOpen = false;
	bool bDirty = false;
};
