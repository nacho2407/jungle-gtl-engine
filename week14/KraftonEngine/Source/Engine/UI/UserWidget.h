#pragma once

#include "Object/Object.h"

class APlayerController;
namespace Rml { class ElementDocument; }

class UUserWidget : public UObject
{
public:
	DECLARE_CLASS(UUserWidget, UObject)

	UUserWidget() = default;
	~UUserWidget() override = default;

	void Initialize(APlayerController* InOwningPlayer, const FString& InDocumentPath);
	void AddToViewport(int32 InZOrder = 0);
	void RemoveFromParent();

	APlayerController* GetOwningPlayer() const { return OwningPlayer; }
	const FString& GetDocumentPath() const { return DocumentPath; }
	int32 GetZOrder() const { return ZOrder; }
	bool IsInViewport() const { return bInViewport; }
	bool IsDocumentLoaded() const { return bDocumentLoaded; }
	Rml::ElementDocument* GetDocument() const { return Document; }

	void MarkDocumentLoaded(Rml::ElementDocument* InDocument) { Document = InDocument; bDocumentLoaded = Document != nullptr; }
	void MarkRemovedFromViewport() { bInViewport = false; }
	void ClearDocument() { Document = nullptr; bDocumentLoaded = false; }

private:
	APlayerController* OwningPlayer = nullptr;
	Rml::ElementDocument* Document = nullptr;
	FString DocumentPath;
	int32 ZOrder = 0;
	bool bInViewport = false;
	bool bDocumentLoaded = false;
};
