#pragma once
#include "Core/ClassTypes.h"
#include "Editor/UI/ContentBrowser/ContentBrowserContext.h"
#include "ContentItem.h"
#include <d3d11.h>

class ContentBrowserElement
{
public:
	virtual ~ContentBrowserElement() = default;
	bool RenderSelectSpace(ContentBrowserContext& Context);
	virtual void Render(ContentBrowserContext& Context);

	void SetIcon(ID3D11ShaderResourceView* InIcon) { Icon = InIcon; }
	void SetContent(FContentItem InContent) { ContentItem = InContent; }

	std::wstring GetFileName() {return ContentItem.Path.filename(); }
protected:
	FString EllipsisText(const FString& text, float maxWidth);

	virtual void OnClicked(ContentBrowserContext& Context) { (void)Context; };
	virtual void OnDoubleClicked(ContentBrowserContext& Context) { (void)Context; };
	virtual void OnDrag(ContentBrowserContext& Context) { ImGui::SetDragDropPayload("ParkSangHyeok", &Context, sizeof(Context)); } //아무 기능 없는 코드입니다

protected:
	ID3D11ShaderResourceView* Icon = nullptr;
	FContentItem ContentItem;
	bool bIsSelected = false;
};

class DirectoryElement final : public ContentBrowserElement
{
public:
	void OnDoubleClicked(ContentBrowserContext& Context) override;
};

class SceneElement final : public ContentBrowserElement
{
public:
	void OnDoubleClicked(ContentBrowserContext& Context) override;
};

class ObjectElement final : public ContentBrowserElement
{
public:
	void OnDrag(ContentBrowserContext& Context) override;
};

//class MaterialElement final : public ContentBrowserElement
//{
//public:
//	void Render(ContentBrowserContext& Context) override;
//};