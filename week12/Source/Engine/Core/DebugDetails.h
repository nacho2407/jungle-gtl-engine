#pragma once

#include "Core/CoreMinimal.h"

#include <functional>
#include <utility>

struct ID3D11ShaderResourceView;

struct FSRVDisplayInfo
{
	float ImageWidth = 256.0f;
	float ImageHeight = 256.0f;
	float UV0X = 0.0f;
	float UV0Y = 0.0f;
	float UV1X = 1.0f;
	float UV1Y = 1.0f;
};

struct FDebugSRVPreviewData
{
	ID3D11ShaderResourceView* SRV = nullptr;
	FSRVDisplayInfo DisplayInfo;
};

struct FDebugCubeSRVPreviewData
{
	ID3D11ShaderResourceView* FaceSRVs[6] = {};
	FSRVDisplayInfo DisplayInfo = { 64.0f, 64.0f, 0.0f, 0.0f, 1.0f, 1.0f };
};

enum class EDebugDetailsItemType : uint8
{
	Text,
	Button,
	SRVPreview,
	CubeSRVPreview,
	TagEditor,
	Custom,
};

struct FDebugDetailsItem
{
	EDebugDetailsItemType Type = EDebugDetailsItemType::Text;
	FString Label;
	FString Value;
	FDebugSRVPreviewData SRVPreview;
	FDebugCubeSRVPreviewData CubeSRVPreview;
	std::function<void()> Callback;
};

class FDebugDetailsBuilder
{
public:
	void AddText(const char* Label, const FString& Value)
	{
		FDebugDetailsItem Item;
		Item.Type = EDebugDetailsItemType::Text;
		Item.Label = Label ? Label : "";
		Item.Value = Value;
		Items.push_back(Item);
	}

	void AddButton(const char* Label, std::function<void()> OnClick)
	{
		FDebugDetailsItem Item;
		Item.Type = EDebugDetailsItemType::Button;
		Item.Label = Label ? Label : "";
		Item.Callback = std::move(OnClick);
		Items.push_back(Item);
	}

	void AddSRVPreview(const char* Label, const FDebugSRVPreviewData& Data)
	{
		FDebugDetailsItem Item;
		Item.Type = EDebugDetailsItemType::SRVPreview;
		Item.Label = Label ? Label : "";
		Item.SRVPreview = Data;
		Items.push_back(Item);
	}

	void AddCubeSRVPreview(const char* Label, const FDebugCubeSRVPreviewData& Data)
	{
		FDebugDetailsItem Item;
		Item.Type = EDebugDetailsItemType::CubeSRVPreview;
		Item.Label = Label ? Label : "";
		Item.CubeSRVPreview = Data;
		Items.push_back(Item);
	}

	void AddTagEditor(const char* Label, std::function<void()> Draw)
	{
		FDebugDetailsItem Item;
		Item.Type = EDebugDetailsItemType::TagEditor;
		Item.Label = Label ? Label : "";
		Item.Callback = std::move(Draw);
		Items.push_back(Item);
	}

	void AddCustom(std::function<void()> Draw)
	{
		FDebugDetailsItem Item;
		Item.Type = EDebugDetailsItemType::Custom;
		Item.Callback = std::move(Draw);
		Items.push_back(Item);
	}

	bool IsEmpty() const { return Items.empty(); }
	const TArray<FDebugDetailsItem>& GetItems() const { return Items; }

private:
	TArray<FDebugDetailsItem> Items;
};
