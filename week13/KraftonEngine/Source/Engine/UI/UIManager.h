#pragma once

#include "Core/CoreTypes.h"
#include "Core/Singleton.h"
#include "Render/Types/RenderTypes.h"

#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#include <RmlUi/Core.h>

#include <chrono>

class APlayerController;
class UUserWidget;
struct FPassContext;
struct ID3D11Buffer;
struct ID3D11Device;
struct ID3D11RasterizerState;
struct ID3D11ShaderResourceView;

class FRmlSystemInterface final : public Rml::SystemInterface
{
public:
	double GetElapsedTime() override;
	void JoinPath(Rml::String& TranslatedPath, const Rml::String& DocumentPath, const Rml::String& Path) override;
	bool LogMessage(Rml::Log::Type Type, const Rml::String& Message) override;

private:
	std::chrono::steady_clock::time_point StartTime = std::chrono::steady_clock::now();
};

class FRmlRenderInterfaceD3D11 final : public Rml::RenderInterface
{
public:
	explicit FRmlRenderInterfaceD3D11(ID3D11Device* InDevice);
	~FRmlRenderInterfaceD3D11() override;

	void BeginFrame(const FPassContext& InCtx) { Ctx = &InCtx; }
	void EndFrame() { Ctx = nullptr; }

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle GeometryHandle, Rml::Vector2f Translation, Rml::TextureHandle Texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle GeometryHandle) override;
	Rml::TextureHandle LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String& Source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions) override;
	void ReleaseTexture(Rml::TextureHandle Texture) override;
	void EnableScissorRegion(bool Enable) override;
	void SetScissorRegion(Rml::Rectanglei Region) override;

private:
	void CreateConstantBuffer();
	void CreateWhiteTexture();
	void ReleaseWhiteTexture();

private:
	ID3D11Device* Device = nullptr;
	ID3D11Buffer* PerFrameCB = nullptr;
	ID3D11ShaderResourceView* WhiteTextureSRV = nullptr;
	ID3D11RasterizerState* ScissorRasterizerState = nullptr;
	const FPassContext* Ctx = nullptr;
};

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
