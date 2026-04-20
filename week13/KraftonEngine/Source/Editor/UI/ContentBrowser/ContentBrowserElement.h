#pragma once
#include "Core/ClassTypes.h"
#include "Editor/UI/ContentBrowser/ContentBrowserContext.h"
#include "ContentItem.h"
#include <d3d11.h>
#include <shellapi.h>


class ContentBrowserElement
{
public:
	virtual ~ContentBrowserElement() = default;
	bool RenderSelectSpace(ContentBrowserContext& Context);
	virtual void Render(ContentBrowserContext& Context);

	void SetIcon(ID3D11ShaderResourceView* InIcon) { Icon = InIcon; }
	void SetContent(FContentItem InContent) { ContentItem = InContent; }

	std::wstring GetFileName() { return ContentItem.Path.filename(); }

protected:
	FString EllipsisText(const FString& text, float maxWidth);
	virtual const char* GetDragItemType() { return "ParkSangHyeok"; }

	virtual void OnRightClicked(ContentBrowserContext& Context) { (void)Context; };
	virtual void OnDoubleRightClicked(ContentBrowserContext& Context) { ShellExecuteW(nullptr, L"open", ContentItem.Path.c_str(), nullptr, nullptr, SW_SHOWNORMAL); };
	virtual void OnDrag(ContentBrowserContext& Context) { (void)Context; }

protected:
	ID3D11ShaderResourceView* Icon = nullptr;
	FContentItem ContentItem;
	bool bIsSelected = false;
};

class DirectoryElement final : public ContentBrowserElement
{
public:
	void OnDoubleRightClicked(ContentBrowserContext& Context) override;
};

class SceneElement final : public ContentBrowserElement
{
public:
	void OnDoubleRightClicked(ContentBrowserContext& Context) override;
};

class ObjectElement final : public ContentBrowserElement
{
public:
	virtual const char* GetDragItemType() override { return "ObjectContentItem"; }
};

//class MaterialElement final : public ContentBrowserElement
//{
//public:
//	void Render(ContentBrowserContext& Context) override;
//};