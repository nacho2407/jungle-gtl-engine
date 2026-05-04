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
struct FFrameContext;
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

// 한글 경로 호환 — RmlUi 기본 FileInterface 는 fopen(UTF-8 bytes) 로 동작하는데, Windows
// fopen 은 인자를 ANSI 코드페이지(예: CP949) 로 해석해 한글 경로의 UTF-8 바이트를 깨뜨린다.
// 이 인터페이스는 항상 wide(_wfopen) 로 열어 한글 경로 디렉토리에서도 RML / 폰트 / CSS 가
// 정상 로드되게 한다.
class FRmlFileInterfaceWide final : public Rml::FileInterface
{
public:
	Rml::FileHandle Open(const Rml::String& Path) override;
	void Close(Rml::FileHandle FileHandle) override;
	size_t Read(void* Buffer, size_t Size, Rml::FileHandle FileHandle) override;
	bool Seek(Rml::FileHandle FileHandle, long Offset, int Origin) override;
	size_t Tell(Rml::FileHandle FileHandle) override;
};

class FRmlRenderInterfaceD3D11 final : public Rml::RenderInterface
{
public:
	explicit FRmlRenderInterfaceD3D11(ID3D11Device* InDevice);
	~FRmlRenderInterfaceD3D11() override;

	void BeginFrame(const FPassContext& InCtx);
	void EndFrame();

	Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices) override;
	void RenderGeometry(Rml::CompiledGeometryHandle GeometryHandle, Rml::Vector2f Translation, Rml::TextureHandle Texture) override;
	void ReleaseGeometry(Rml::CompiledGeometryHandle GeometryHandle) override;
	Rml::TextureHandle LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String& Source) override;
	Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions) override;
	void ReleaseTexture(Rml::TextureHandle Texture) override;
	void EnableScissorRegion(bool Enable) override;
	void SetScissorRegion(Rml::Rectanglei Region) override;
	void SetTransform(const Rml::Matrix4f* Transform) override;

private:
	void CreateConstantBuffer();
	void CreateWhiteTexture();
	void ReleaseWhiteTexture();

private:
	ID3D11Device* Device = nullptr;
	ID3D11Buffer* PerFrameCB = nullptr;
	ID3D11ShaderResourceView* WhiteTextureSRV = nullptr;
	ID3D11RasterizerState* ScissorRasterizerState = nullptr;
	Rml::Matrix4f CurrentTransform;
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
	// PIE end / TransitionToScene 같은 라이프사이클 경계 — viewport 만 비우고 widget UObject
	// 는 유지 (Lua 가 캐시한 핸들이 valid 한 채로 다음 세션에 재사용되도록).
	void ClearViewport();
	// 엔진 shutdown 전용 — 모든 widget UObject 파괴.
	void DestroyAllWidgets();

	void Render(const FPassContext& Ctx);
	bool HasViewportWidgets() const { return !ViewportWidgets.empty(); }

	// viewport 에 올라온 widget 중 하나라도 WantsMouse() == true 면 시스템 커서를 보이고
	// raw mouse / clip 을 풀어야 한다 — GameViewportClient::ProcessInput 이 폴링해서 사용.
	bool AnyViewportWidgetWantsMouse() const;

private:
	UUIManager() = default;
	~UUIManager() = default;

	bool LoadDocument(UUserWidget* Widget);
	void CloseDocument(UUserWidget* Widget);
	void ProcessInput(const FFrameContext& Frame);
	void RemoveFromViewportImmediate(UUserWidget* Widget);
	void FlushDeferredViewportRemovals();

private:
	TArray<UUserWidget*> ViewportWidgets;
	TArray<UUserWidget*> CreatedWidgets;
	TArray<UUserWidget*> PendingRemoveWidgets;

	ID3D11Device* CachedDevice = nullptr;
	FRmlSystemInterface* SystemInterface = nullptr;
	FRmlFileInterfaceWide* FileInterface = nullptr;
	FRmlRenderInterfaceD3D11* RenderInterface = nullptr;
	Rml::Context* RmlContext = nullptr;
	bool bRmlInitialized = false;
	bool bDispatchingRmlEvents = false;
};
