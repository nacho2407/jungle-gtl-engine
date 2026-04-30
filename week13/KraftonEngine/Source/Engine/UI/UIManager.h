#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Render/Types/RenderTypes.h"

class APlayerController;
class UUserWidget;
struct FPassContext;
namespace Rml { class Context; }

class FRmlRenderInterfaceD3D11;
class FRmlSystemInterface;

class UUIManager : public TSingleton<UUIManager>
{
	friend class TSingleton<UUIManager>;

public:
	void Initialize(ID3D11Device* InDevice);
	void Shutdown();

	UUserWidget* CreateWidget(APlayerController* OwningPlayer, const FString& DocumentPath);
	void AddToViewport(UUserWidget* Widget, int32 ZOrder);
	void RemoveFromViewport(UUserWidget* Widget);
	void ClearViewport();

	void Render(const FPassContext& Ctx);
	bool HasViewportWidgets() const { return !ViewportWidgets.empty(); }

private:
	UUIManager() = default;
	~UUIManager() = default;

	bool LoadDocument(UUserWidget* Widget);
	void CloseDocument(UUserWidget* Widget);

private:
	TArray<UUserWidget*> ViewportWidgets;
	TArray<UUserWidget*> CreatedWidgets;

	ID3D11Device* CachedDevice = nullptr;
	FRmlSystemInterface* SystemInterface = nullptr;
	FRmlRenderInterfaceD3D11* RenderInterface = nullptr;
	Rml::Context* RmlContext = nullptr;
	bool bRmlInitialized = false;
};
