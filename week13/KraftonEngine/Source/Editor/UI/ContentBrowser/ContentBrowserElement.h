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
	virtual void RenderDetail();

	virtual void RenderContextMenu(ContentBrowserContext& Context) {}

	void SetIcon(ID3D11ShaderResourceView* InIcon) { Icon = InIcon; }
	void SetContent(FContentItem InContent) { ContentItem = InContent; }

	std::wstring GetFileName() { return ContentItem.Path.filename(); }

protected:
	FString EllipsisText(const FString& text, float maxWidth);

	virtual FString GetDisplayName() const;
	virtual const char* GetTypeLabel() const { return ""; }
	virtual const char* GetDragItemType() { return "ParkSangHyeok"; }

	virtual uint32 GetAccentColor() const { return 0; }

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
	void RenderContextMenu(ContentBrowserContext& Context) override;

	virtual const char* GetDragItemType() override { return "ObjectContentItem"; }

protected:
	const char* GetTypeLabel() const override { return "Static Mesh"; }
	uint32 GetAccentColor() const override { return IM_COL32(88, 160, 230, 255); }
};

class FloatCurveElement final : public ContentBrowserElement
{
public:
	virtual const char* GetDragItemType() override { return "FloatCurveContentItem"; }
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Float Curve"; }
	uint32 GetAccentColor() const override { return IM_COL32(90, 190, 120, 255); }
};

class CameraShakeElement final : public ContentBrowserElement
{
public:
	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Camera Shake"; }
	uint32 GetAccentColor() const override { return IM_COL32(230, 150, 75, 255); }
};

class MeshElement final : public ContentBrowserElement
{
public:
	void RenderContextMenu(ContentBrowserContext& Context) override;

	void OnDoubleLeftClicked(ContentBrowserContext& Context) override;

protected:
	const char* GetTypeLabel() const override { return "Skeletal Mesh"; }
	uint32 GetAccentColor() const override { return IM_COL32(126, 140, 255, 255); }
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
