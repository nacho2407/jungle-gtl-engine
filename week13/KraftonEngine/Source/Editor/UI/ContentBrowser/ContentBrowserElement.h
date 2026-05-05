#pragma once
#include "Core/ClassTypes.h"
#include "Editor/UI/ContentBrowser/ContentBrowserContext.h"
#include "ContentItem.h"
#include <d3d11.h>
#include <shellapi.h>


class ContentBrowserElement : public std::enable_shared_from_this<ContentBrowserElement>
{
public:
	virtual ~ContentBrowserElement() = default;
	bool RenderSelectSpace(ContentBrowserContext& Context);
	virtual void Render(ContentBrowserContext& Context);
	virtual void RenderDetail() {};

	void SetIcon(ID3D11ShaderResourceView* InIcon) { Icon = InIcon; }
	void SetContent(FContentItem InContent) { ContentItem = InContent; }

	std::wstring GetFileName() { return ContentItem.Path.filename(); }

protected:
	FString EllipsisText(const FString& text, float maxWidth);
	virtual const char* GetDragItemType() { return "ParkSangHyeok"; }

	virtual void OnLeftClicked(ContentBrowserContext& Context) { (void)Context; };
	virtual void OnDoubleLeftClicked(ContentBrowserContext& Context) { ShellExecuteW(nullptr, L"open", ContentItem.Path.c_str(), nullptr, nullptr, SW_SHOWNORMAL); };
	virtual void OnDrag(ContentBrowserContext& Context) { (void)Context; }

protected:
	ID3D11ShaderResourceView* Icon = nullptr;
	FContentItem ContentItem;
	bool bIsSelected = false;
};

class DirectoryElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;
};

class SceneElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;
};

class ObjectElement final : public ContentBrowserElement
{
public:
	virtual const char* GetDragItemType() override { return "ObjectContentItem"; }
};

class FloatCurveElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;
};

class PNGElement final : public ContentBrowserElement
{
public:
	virtual const char* GetDragItemType() override { return "PNGElement"; }
};

#include "Editor/UI/EditorMaterialInspector.h"
class MaterialElement final : public ContentBrowserElement
{
public:
	virtual void OnLeftClicked(ContentBrowserContext& Context) override;
	virtual const char* GetDragItemType() override { return "MaterialContentItem"; }
	virtual void RenderDetail() override;

private:
	FEditorMaterialInspector MaterialInspector;
};