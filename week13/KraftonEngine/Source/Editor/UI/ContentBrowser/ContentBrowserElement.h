#pragma once
#include "Core/ClassTypes.h"
#include "Editor/UI/ContentBrowser/ContentBrowserContext.h"
#include "ContentItem.h"
#include <d3d11.h>

class ContentBrowserElement
{
public:
	virtual ~ContentBrowserElement() = default;
	virtual void Render(ContentBrowserContext& Context) = 0;
	void SetIcon(ID3D11ShaderResourceView* InIcon) { Icon = InIcon; }
	void SetContent(FContentItem InContent) { ContentItem = InContent; }

protected:
	FString EllipsisText(const FString& text, float maxWidth);

protected:
	bool bIsSelected = false;
	ID3D11ShaderResourceView* Icon = nullptr;
	FContentItem ContentItem;
};

class DefaultElement final : public ContentBrowserElement
{
public:
	void Render(ContentBrowserContext& Context) override;
};

class DirectoryElement final : public ContentBrowserElement
{
public:
	void Render(ContentBrowserContext& Context) override;
};