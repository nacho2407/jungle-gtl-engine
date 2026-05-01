#include "UI/UIManager.h"

#include "Core/Log.h"
#include "Object/Object.h"
#include "Platform/Paths.h"
#include "Render/Command/DrawCommandList.h"
#include "Render/Device/D3DDevice.h"
#include "Render/RenderPass/RenderPassBase.h"
#include "Render/Resource/RenderResources.h"
#include "Render/Shader/ShaderManager.h"
#include "UI/UserWidget.h"

#ifdef GetNextSibling
#undef GetNextSibling
#endif
#ifdef GetFirstChild
#undef GetFirstChild
#endif
#include <RmlUi/Core.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <memory>

namespace
{
	struct FRmlVertexD3D11
	{
		float X, Y;
		float R, G, B, A;
		float U, V;
	};

	struct FRmlGeometryD3D11
	{
		ID3D11Buffer* VertexBuffer = nullptr;
		ID3D11Buffer* IndexBuffer = nullptr;
		UINT IndexCount = 0;
	};

	struct FRmlTextureD3D11
	{
		ID3D11ShaderResourceView* SRV = nullptr;
	};

	struct FRmlPerFrameCB
	{
		float ViewportWidth = 1.0f;
		float ViewportHeight = 1.0f;
		float TranslationX = 0.0f;
		float TranslationY = 0.0f;
	};

	constexpr const char* UIShaderPath = "Shaders/UI/RmlUi.hlsl";

	std::filesystem::path ToProjectPath(const FString& Path)
	{
		std::filesystem::path Result(FPaths::ToWide(Path));
		if (Result.is_relative())
		{
			Result = std::filesystem::path(FPaths::RootDir()) / Result;
		}
		return Result;
	}

	Rml::String ToRmlPath(const std::filesystem::path& Path)
	{
		return FPaths::ToUtf8(Path.generic_wstring());
	}
}

double FRmlSystemInterface::GetElapsedTime()
{
	using namespace std::chrono;
	const auto Now = steady_clock::now();
	return duration<double>(Now - StartTime).count();
}

void FRmlSystemInterface::JoinPath(Rml::String& TranslatedPath, const Rml::String& DocumentPath, const Rml::String& Path)
{
	std::filesystem::path ResourcePath(FPaths::ToWide(Path));
	if (!ResourcePath.is_relative())
	{
		TranslatedPath = ToRmlPath(ResourcePath);
		return;
	}

	std::filesystem::path BasePath(FPaths::ToWide(DocumentPath));
	TranslatedPath = ToRmlPath(BasePath.parent_path() / ResourcePath);
}

bool FRmlSystemInterface::LogMessage(Rml::Log::Type Type, const Rml::String& Message)
{
	UE_LOG("[RmlUi] %s", Message.c_str());
	return Type != Rml::Log::LT_ASSERT;
}

FRmlRenderInterfaceD3D11::FRmlRenderInterfaceD3D11(ID3D11Device* InDevice)
	: Device(InDevice)
{
	CreateConstantBuffer();
}

FRmlRenderInterfaceD3D11::~FRmlRenderInterfaceD3D11()
{
	ReleaseWhiteTexture();
	if (ScissorRasterizerState)
	{
		ScissorRasterizerState->Release();
		ScissorRasterizerState = nullptr;
	}
	if (PerFrameCB)
	{
		PerFrameCB->Release();
		PerFrameCB = nullptr;
	}
}

Rml::CompiledGeometryHandle FRmlRenderInterfaceD3D11::CompileGeometry(Rml::Span<const Rml::Vertex> Vertices, Rml::Span<const int> Indices)
{
	if (!Device || Vertices.empty() || Indices.empty())
	{
		return 0;
	}

	TArray<FRmlVertexD3D11> ConvertedVertices;
	ConvertedVertices.reserve(Vertices.size());
	for (const Rml::Vertex& Vertex : Vertices)
	{
		ConvertedVertices.push_back({
			Vertex.position.x,
			Vertex.position.y,
			Vertex.colour.red / 255.0f,
			Vertex.colour.green / 255.0f,
			Vertex.colour.blue / 255.0f,
			Vertex.colour.alpha / 255.0f,
			Vertex.tex_coord.x,
			Vertex.tex_coord.y,
		});
	}

	TArray<uint32> ConvertedIndices;
	ConvertedIndices.reserve(Indices.size());
	for (int Index : Indices)
	{
		ConvertedIndices.push_back(static_cast<uint32>(Index));
	}

	auto* Geometry = new FRmlGeometryD3D11();
	Geometry->IndexCount = static_cast<UINT>(ConvertedIndices.size());

	D3D11_BUFFER_DESC VBDesc = {};
	VBDesc.Usage = D3D11_USAGE_DEFAULT;
	VBDesc.ByteWidth = static_cast<UINT>(sizeof(FRmlVertexD3D11) * ConvertedVertices.size());
	VBDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VBData = {};
	VBData.pSysMem = ConvertedVertices.data();
	if (FAILED(Device->CreateBuffer(&VBDesc, &VBData, &Geometry->VertexBuffer)))
	{
		delete Geometry;
		return 0;
	}

	D3D11_BUFFER_DESC IBDesc = {};
	IBDesc.Usage = D3D11_USAGE_DEFAULT;
	IBDesc.ByteWidth = static_cast<UINT>(sizeof(uint32) * ConvertedIndices.size());
	IBDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IBData = {};
	IBData.pSysMem = ConvertedIndices.data();
	if (FAILED(Device->CreateBuffer(&IBDesc, &IBData, &Geometry->IndexBuffer)))
	{
		ReleaseGeometry(reinterpret_cast<Rml::CompiledGeometryHandle>(Geometry));
		return 0;
	}

	return reinterpret_cast<Rml::CompiledGeometryHandle>(Geometry);
}

void FRmlRenderInterfaceD3D11::RenderGeometry(Rml::CompiledGeometryHandle GeometryHandle, Rml::Vector2f Translation, Rml::TextureHandle Texture)
{
	if (!Ctx || !GeometryHandle)
	{
		return;
	}

	auto* Geometry = reinterpret_cast<FRmlGeometryD3D11*>(GeometryHandle);
	ID3D11DeviceContext* DC = Ctx->Device.GetDeviceContext();
	if (!DC || !Geometry->VertexBuffer || !Geometry->IndexBuffer)
	{
		return;
	}

	FShader* Shader = FShaderManager::Get().GetOrCreate(UIShaderPath);
	if (!Shader || !Shader->IsValid())
	{
		return;
	}

	Ctx->Resources.SetDepthStencilState(Ctx->Device, EDepthStencilState::NoDepth);
	Ctx->Resources.SetBlendState(Ctx->Device, EBlendState::AlphaBlend);
	Ctx->Resources.SetRasterizerState(Ctx->Device, ERasterizerState::SolidNoCull);

	DC->OMSetRenderTargets(1, &Ctx->Cache.RTV, Ctx->Cache.DSV);
	DC->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Shader->Bind(DC);

	FRmlPerFrameCB CBData;
	CBData.ViewportWidth = Ctx->Frame.ViewportWidth;
	CBData.ViewportHeight = Ctx->Frame.ViewportHeight;
	CBData.TranslationX = Translation.x;
	CBData.TranslationY = Translation.y;
	DC->UpdateSubresource(PerFrameCB, 0, nullptr, &CBData, 0, 0);
	DC->VSSetConstantBuffers(0, 1, &PerFrameCB);

	ID3D11ShaderResourceView* SRV = WhiteTextureSRV;
	if (Texture)
	{
		auto* TextureResource = reinterpret_cast<FRmlTextureD3D11*>(Texture);
		SRV = TextureResource ? TextureResource->SRV : nullptr;
	}
	DC->PSSetShaderResources(0, 1, &SRV);

	UINT Stride = sizeof(FRmlVertexD3D11);
	UINT Offset = 0;
	DC->IASetVertexBuffers(0, 1, &Geometry->VertexBuffer, &Stride, &Offset);
	DC->IASetIndexBuffer(Geometry->IndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DC->DrawIndexed(Geometry->IndexCount, 0, 0);
}

void FRmlRenderInterfaceD3D11::ReleaseGeometry(Rml::CompiledGeometryHandle GeometryHandle)
{
	auto* Geometry = reinterpret_cast<FRmlGeometryD3D11*>(GeometryHandle);
	if (!Geometry)
	{
		return;
	}

	if (Geometry->VertexBuffer)
	{
		Geometry->VertexBuffer->Release();
	}
	if (Geometry->IndexBuffer)
	{
		Geometry->IndexBuffer->Release();
	}
	delete Geometry;
}

Rml::TextureHandle FRmlRenderInterfaceD3D11::LoadTexture(Rml::Vector2i& TextureDimensions, const Rml::String& /*Source*/)
{
	TextureDimensions = { 0, 0 };
	return 0;
}

Rml::TextureHandle FRmlRenderInterfaceD3D11::GenerateTexture(Rml::Span<const Rml::byte> Source, Rml::Vector2i SourceDimensions)
{
	if (!Device || Source.empty() || SourceDimensions.x <= 0 || SourceDimensions.y <= 0)
	{
		return 0;
	}

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = static_cast<UINT>(SourceDimensions.x);
	TextureDesc.Height = static_cast<UINT>(SourceDimensions.y);
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = Source.data();
	InitialData.SysMemPitch = static_cast<UINT>(SourceDimensions.x * 4);

	ID3D11Texture2D* Texture = nullptr;
	if (FAILED(Device->CreateTexture2D(&TextureDesc, &InitialData, &Texture)))
	{
		return 0;
	}

	ID3D11ShaderResourceView* SRV = nullptr;
	HRESULT HR = Device->CreateShaderResourceView(Texture, nullptr, &SRV);
	Texture->Release();
	if (FAILED(HR))
	{
		return 0;
	}

	auto* TextureResource = new FRmlTextureD3D11();
	TextureResource->SRV = SRV;
	return reinterpret_cast<Rml::TextureHandle>(TextureResource);
}

void FRmlRenderInterfaceD3D11::ReleaseTexture(Rml::TextureHandle Texture)
{
	auto* TextureResource = reinterpret_cast<FRmlTextureD3D11*>(Texture);
	if (!TextureResource)
	{
		return;
	}
	if (TextureResource->SRV)
	{
		TextureResource->SRV->Release();
	}
	delete TextureResource;
}

void FRmlRenderInterfaceD3D11::EnableScissorRegion(bool Enable)
{
	if (!Ctx)
	{
		return;
	}

	ID3D11DeviceContext* DC = Ctx->Device.GetDeviceContext();
	if (Enable && ScissorRasterizerState)
	{
		DC->RSSetState(ScissorRasterizerState);
	}
	else
	{
		Ctx->Resources.SetRasterizerState(Ctx->Device, ERasterizerState::SolidNoCull);
	}

	if (!Enable)
	{
		DC->RSSetScissorRects(0, nullptr);
	}
}

void FRmlRenderInterfaceD3D11::SetScissorRegion(Rml::Rectanglei Region)
{
	if (!Ctx)
	{
		return;
	}

	D3D11_RECT Rect = {};
	Rect.left = Region.Left();
	Rect.top = Region.Top();
	Rect.right = Region.Right();
	Rect.bottom = Region.Bottom();
	Ctx->Device.GetDeviceContext()->RSSetScissorRects(1, &Rect);
}

void FRmlRenderInterfaceD3D11::CreateConstantBuffer()
{
	if (!Device)
	{
		return;
	}

	D3D11_BUFFER_DESC Desc = {};
	Desc.Usage = D3D11_USAGE_DEFAULT;
	Desc.ByteWidth = sizeof(FRmlPerFrameCB);
	Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	Device->CreateBuffer(&Desc, nullptr, &PerFrameCB);

	CreateWhiteTexture();

	D3D11_RASTERIZER_DESC RasterDesc = {};
	RasterDesc.FillMode = D3D11_FILL_SOLID;
	RasterDesc.CullMode = D3D11_CULL_NONE;
	RasterDesc.ScissorEnable = TRUE;
	Device->CreateRasterizerState(&RasterDesc, &ScissorRasterizerState);
}

void FRmlRenderInterfaceD3D11::CreateWhiteTexture()
{
	const uint32 WhitePixel = 0xffffffff;

	D3D11_TEXTURE2D_DESC TextureDesc = {};
	TextureDesc.Width = 1;
	TextureDesc.Height = 1;
	TextureDesc.MipLevels = 1;
	TextureDesc.ArraySize = 1;
	TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	TextureDesc.SampleDesc.Count = 1;
	TextureDesc.Usage = D3D11_USAGE_DEFAULT;
	TextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA InitialData = {};
	InitialData.pSysMem = &WhitePixel;
	InitialData.SysMemPitch = sizeof(uint32);

	ID3D11Texture2D* Texture = nullptr;
	if (SUCCEEDED(Device->CreateTexture2D(&TextureDesc, &InitialData, &Texture)))
	{
		Device->CreateShaderResourceView(Texture, nullptr, &WhiteTextureSRV);
		Texture->Release();
	}
}

void FRmlRenderInterfaceD3D11::ReleaseWhiteTexture()
{
	if (WhiteTextureSRV)
	{
		WhiteTextureSRV->Release();
		WhiteTextureSRV = nullptr;
	}
}

void UUIManager::Initialize(ID3D11Device* InDevice)
{
	CachedDevice = InDevice;

	if (bRmlInitialized || !CachedDevice)
	{
		return;
	}

	SystemInterface = new FRmlSystemInterface();
	RenderInterface = new FRmlRenderInterfaceD3D11(CachedDevice);

	Rml::SetSystemInterface(SystemInterface);
	Rml::SetRenderInterface(RenderInterface);
	bRmlInitialized = Rml::Initialise();
	if (!bRmlInitialized)
	{
		UE_LOG("[RmlUi] Initialise failed.");
		return;
	}

	RmlContext = Rml::CreateContext("GameViewport", Rml::Vector2i(1, 1));
	if (!RmlContext)
	{
		UE_LOG("[RmlUi] Failed to create GameViewport context.");
	}

	const std::filesystem::path FontPath = ToProjectPath("Asset/Font/Maplestory Bold.ttf");
	if (!Rml::LoadFontFace(ToRmlPath(FontPath), "Maplestory", Rml::Style::FontStyle::Normal, Rml::Style::FontWeight::Bold))
	{
		UE_LOG("[RmlUi] Failed to load font: Asset/Font/Maplestory Bold.ttf");
	}
}

void UUIManager::Shutdown()
{
	ClearViewport();

	if (RmlContext)
	{
		Rml::RemoveContext("GameViewport");
		RmlContext = nullptr;
	}

	if (bRmlInitialized)
	{
		Rml::Shutdown();
		bRmlInitialized = false;
	}

	delete RenderInterface;
	RenderInterface = nullptr;
	delete SystemInterface;
	SystemInterface = nullptr;
	CachedDevice = nullptr;
}

UUserWidget* UUIManager::CreateWidget(APlayerController* OwningPlayer, const FString& DocumentPath)
{
	UUserWidget* Widget = UObjectManager::Get().CreateObject<UUserWidget>();
	Widget->Initialize(OwningPlayer, DocumentPath);
	CreatedWidgets.push_back(Widget);
	return Widget;
}

void UUIManager::AddToViewport(UUserWidget* Widget, int32 /*ZOrder*/)
{
	if (!Widget)
	{
		return;
	}

	if (!LoadDocument(Widget))
	{
		return;
	}

	auto It = std::find(ViewportWidgets.begin(), ViewportWidgets.end(), Widget);
	if (It == ViewportWidgets.end())
	{
		ViewportWidgets.push_back(Widget);
	}

	std::sort(ViewportWidgets.begin(), ViewportWidgets.end(),
		[](const UUserWidget* A, const UUserWidget* B)
		{
			return A->GetZOrder() < B->GetZOrder();
		});
}

void UUIManager::RemoveFromViewport(UUserWidget* Widget)
{
	ViewportWidgets.erase(std::remove(ViewportWidgets.begin(), ViewportWidgets.end(), Widget), ViewportWidgets.end());
	CloseDocument(Widget);
	if (Widget)
	{
		Widget->MarkRemovedFromViewport();
	}
}

void UUIManager::ClearViewport()
{
	for (UUserWidget* Widget : ViewportWidgets)
	{
		CloseDocument(Widget);
		if (Widget)
		{
			Widget->MarkRemovedFromViewport();
		}
	}
	ViewportWidgets.clear();

	if (RmlContext)
	{
		RmlContext->Update();
	}

	for (UUserWidget* Widget : CreatedWidgets)
	{
		if (IsAliveObject(Widget))
		{
			UObjectManager::Get().DestroyObject(Widget);
		}
	}
	CreatedWidgets.clear();
}

bool UUIManager::LoadDocument(UUserWidget* Widget)
{
	if (!Widget)
	{
		return false;
	}
	if (Widget->IsDocumentLoaded())
	{
		return true;
	}
	if (!RmlContext)
	{
		return false;
	}

	const std::filesystem::path Path = ToProjectPath(Widget->GetDocumentPath());
	if (!std::filesystem::exists(Path))
	{
		UE_LOG("[RmlUi] Document not found: %s", Widget->GetDocumentPath().c_str());
		return false;
	}

	Rml::ElementDocument* Document = RmlContext->LoadDocument(ToRmlPath(Path));
	if (!Document)
	{
		UE_LOG("[RmlUi] Failed to load document: %s", Widget->GetDocumentPath().c_str());
		return false;
	}

	Document->Show();
	Widget->MarkDocumentLoaded(Document);
	return true;
}

void UUIManager::CloseDocument(UUserWidget* Widget)
{
	if (!Widget || !Widget->GetDocument())
	{
		return;
	}

	Widget->GetDocument()->Close();
	Widget->ClearDocument();
}

void UUIManager::Render(const FPassContext& Ctx)
{
	if (!RmlContext || !RenderInterface || ViewportWidgets.empty() || Ctx.Frame.ViewportWidth <= 0.0f || Ctx.Frame.ViewportHeight <= 0.0f)
	{
		return;
	}

	RmlContext->SetDimensions({
		static_cast<int>(Ctx.Frame.ViewportWidth),
		static_cast<int>(Ctx.Frame.ViewportHeight)
	});

	RmlContext->Update();
	RenderInterface->BeginFrame(Ctx);
	RmlContext->Render();
	RenderInterface->EndFrame();
}
