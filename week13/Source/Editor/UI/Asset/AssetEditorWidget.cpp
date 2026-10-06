#include "AssetEditorWidget.h"

void FAssetEditorWidget::Open(UObject* Object)
{
	if (!CanEdit(Object))
	{
		return;
	}

	EditedObject = Object;
	bOpen = true;
	RequestFocus();
	ClearDirty();
}

void FAssetEditorWidget::Close()
{
	EditedObject = nullptr;
	bOpen = false;
	bFocusRequested = false;
	ClearDirty();
}
