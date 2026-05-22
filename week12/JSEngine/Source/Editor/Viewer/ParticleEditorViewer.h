#pragma once

#include "Editor/Viewer/EditorViewer.h"

class FParticleEditorViewer : public FEditorViewer
{
public:
	bool ChangeTarget(const FString& InFileName) override;
	EEditorTabKind GetTabKind() const override;
	const char* GetViewerLabel() const override;
};
