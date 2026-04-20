#pragma once
#include "Render/Types/RenderTypes.h"
#include "Editor/UI/EditorDragSource.h"
#include "Editor/UI/ContentBrowser/ContentBrowserElement.h"
#include "imgui.h"

class PrefabDragSource final : public EditorDragSource
{
public:
	~PrefabDragSource() { delete ActorImage; }

	ID3D11Texture2D* GetImage() const { return ActorImage; }
	void SetImage(ID3D11Texture2D* InImage) { ActorImage = InImage; }

private:
	void RenderSource(ImVec2 InSize) override { ImGui::Image(ActorImage, InSize); }

private:
	ImVec2 Size;
	ID3D11Texture2D* ActorImage;

	//ToDo:Renderer에서 Direct로 ActorImage 생성
};

class EditorPrefabWidget : public ContentBrowserElement
{
public:
	virtual ~EditorPrefabWidget() { delete DragSource; }

	virtual void Render(ContentBrowserContext& Context) override { DragSource->Render(Context.ContentSize); }
	
private:
	PrefabDragSource* DragSource;
};

