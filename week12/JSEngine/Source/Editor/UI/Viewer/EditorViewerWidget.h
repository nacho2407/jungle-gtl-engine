#pragma once

#include "Editor/UI/EditorWidget.h"

class FSceneViewport;
class FEditorViewer;
struct ID3D11ShaderResourceView;
struct ImVec2;

class FEditorViewerWidget : public FEditorWidget
{
public:
	~FEditorViewerWidget() override = default;

	void Initialize(UEditorEngine* InEditorEngine) override;
	void Render(float DeltaTime) override;
	void RenderEmbedded(float DeltaTime);

	void SetViewer(FEditorViewer* InViewer) { Viewer = InViewer; }
	FEditorViewer* GetViewer() const { return Viewer; }

	bool IsOpen() const { return bOpen; }
	void SetOpen(bool NewOpen) { bOpen = NewOpen; }

	FString GetWindowName() const;
	virtual void RequestSaveMesh();
	virtual bool CanSaveMesh() const;
	virtual bool IsMeshDirty() const;

protected:
	virtual void RenderContent(float DeltaTime);
	void RenderViewportPanel(FSceneViewport& SceneViewport, ID3D11ShaderResourceView* SRV, const ImVec2& Size);
	void RenderDetachedDocumentChrome(bool& bDockRequested, bool& bCloseRequested);
	void RenderDetachedDocumentToolbar(bool& bDockRequested);
	void Shutdown();

	FEditorViewer* Viewer = nullptr;
	bool bOpen = false;

	float LeftPanelWidth = 250.0f;
	float RightPanelWidth = 250.0f;
};
