#pragma once
#include "Core/ClassTypes.h"
#include "Editor/UI/ContentBrowser/ContentBrowserContext.h"
#include "ContentItem.h"
#include <d3d11.h>

class ContentBrowserElement
{
public:
	virtual ~ContentBrowserElement() = default;
	virtual void Render(ContentBrowserContext& Context);

	void SetIcon(ID3D11ShaderResourceView* InIcon) { Icon = InIcon; }
	void SetContent(FContentItem InContent) { ContentItem = InContent; }

	std::wstring GetFileName() {return ContentItem.Path.filename(); }
protected:
	FString EllipsisText(const FString& text, float maxWidth);

	virtual void OnClicked(ContentBrowserContext& Context) {};
	virtual void OnDoubleClicked(ContentBrowserContext& Context) {};
	virtual void OnDrag(ContentBrowserContext& Context) {};

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

//class ObjectElement final : public ContentBrowserElement
//{
//public:
//	void Render(ContentBrowserContext& Context) override;
//};
//
//class MaterialElement final : public ContentBrowserElement
//{
//public:
//	void Render(ContentBrowserContext& Context) override;
//};