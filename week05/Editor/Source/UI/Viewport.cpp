#include "Viewport.h"

#include "EditorViewportClient.h"
#include "Core/Core.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "Camera/Camera.h"

#include "imgui.h"

namespace
{
	void ReleaseIfValid(IUnknown*& Resource)
	{
		if (Resource)
		{
			Resource->Release();
			Resource = nullptr;
		}
	}

	bool RenderGizmoModeButton(const char* Label, EGizmoMode Mode, CEditorViewportClient* ViewportClient)
	{
		if (ViewportClient == nullptr)
		{
			return false;
		}

		const bool bSelected = (ViewportClient->GetGizmoMode() == Mode);
		const float ButtonHeight = ImGui::GetFrameHeight();
		const ImVec2 ButtonSize(ButtonHeight, ButtonHeight);
		if (bSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.25f, 0.45f, 0.85f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.35f, 0.55f, 0.95f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.40f, 0.80f, 1.0f));
		}

		const bool bClicked = ImGui::Button(Label, ButtonSize);

		if (bSelected)
		{
			ImGui::PopStyleColor(3);
		}

		return bClicked;
	}
}

CViewport::~CViewport()
{
	ReleaseSceneView();
}

void CViewport::Render(CCore* Core, CRenderer* Renderer, HWND Hwnd)
{
	//ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	const bool bOpen = ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_MenuBar);
	//ImGui::PopStyleVar();

	if (!bOpen)
	{
		bHovered = false;
		bFocused = false;
		bVisible = false;
		if (Renderer)
		{
			Renderer->ClearSceneRenderTarget();
		}
		ImGui::End();
		return;
	}

	if (ImGui::BeginMenuBar())
	{
		CEditorViewportClient* EditorViewportClient = Core ? dynamic_cast<CEditorViewportClient*>(Core->GetViewportClient()) : nullptr;
		if (EditorViewportClient)
		{
			// 도구 선택 버튼
			ImGui::Separator();
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));
			if (RenderGizmoModeButton("T", EGizmoMode::Location, EditorViewportClient))
			{
				EditorViewportClient->SetGizmoMode(EGizmoMode::Location);
			}

			if (RenderGizmoModeButton("R", EGizmoMode::Rotation, EditorViewportClient))
			{
				EditorViewportClient->SetGizmoMode(EGizmoMode::Rotation);
			}

			if (RenderGizmoModeButton("S", EGizmoMode::Scale, EditorViewportClient))
			{
				EditorViewportClient->SetGizmoMode(EGizmoMode::Scale);
			}
			ImGui::PopStyleVar();
		
			// RenderMode 선택
			float RenderModeComboWidth = 120.0f;
			// 오른쪽 정렬
			ImGui::SetCursorPosX(ImGui::GetWindowWidth() - RenderModeComboWidth);
			ImGui::SetNextItemWidth(RenderModeComboWidth);
			{
				ERenderMode RenderMode = EditorViewportClient->GetRenderMode();
				ImGui::Combo("", (int*) &RenderMode, "Lighting\0No Lighting\0Wireframe", 3);
				EditorViewportClient->SetRenderMode(RenderMode);
			}
		}
		ImGui::EndMenuBar();
	}

	bFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	bHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

	const ImVec2 ContentPos = ImGui::GetCursorScreenPos();
	const ImVec2 ContentSize = ImGui::GetContentRegionAvail();
	const uint32 NewWidth = ContentSize.x > 1.0f ? static_cast<uint32>(ContentSize.x) : 0;
	const uint32 NewHeight = ContentSize.y > 1.0f ? static_cast<uint32>(ContentSize.y) : 0;

	bVisible = (NewWidth > 0 && NewHeight > 0);

	if (Hwnd)
	{
		POINT ClientPoint = {
			static_cast<LONG>(ContentPos.x),
			static_cast<LONG>(ContentPos.y)
		};
		::ScreenToClient(Hwnd, &ClientPoint);
		ClientPosX = ClientPoint.x;
		ClientPosY = ClientPoint.y;
	}
	else
	{
		ClientPosX = 0;
		ClientPosY = 0;
	}

	if (!bVisible)
	{
		ReleaseSceneView();
		if (Renderer)
		{
			Renderer->ClearSceneRenderTarget();
		}
		ImGui::End();
		return;
	}

	if (Renderer)
	{
		ReadySceneView(Renderer->GetDevice(), NewWidth, NewHeight);

		if (RenderTargetView && DepthStencilView)
		{
			D3D11_VIEWPORT SceneViewport = {};
			SceneViewport.TopLeftX = 0.0f;
			SceneViewport.TopLeftY = 0.0f;
			SceneViewport.Width = static_cast<float>(NewWidth);
			SceneViewport.Height = static_cast<float>(NewHeight);
			SceneViewport.MinDepth = 0.0f;
			SceneViewport.MaxDepth = 1.0f;
			Renderer->SetSceneRenderTarget(RenderTargetView, DepthStencilView, SceneViewport);
		}
		else
		{
			Renderer->ClearSceneRenderTarget();
		}
	}

	if (Core && Core->GetScene() && Core->GetScene()->GetCamera())
	{
		Core->GetScene()->GetCamera()->SetAspectRatio(static_cast<float>(NewWidth) / static_cast<float>(NewHeight));
	}

	if (ShaderResourceView)
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(ShaderResourceView), ImVec2(static_cast<float>(NewWidth), static_cast<float>(NewHeight)));
	}

	ImGui::End();
}

void CViewport::ReleaseSceneView()
{
	IUnknown* Resource = reinterpret_cast<IUnknown*>(DepthStencilView);
	ReleaseIfValid(Resource);
	DepthStencilView = nullptr;

	Resource = reinterpret_cast<IUnknown*>(DepthStencilTexture);
	ReleaseIfValid(Resource);
	DepthStencilTexture = nullptr;

	Resource = reinterpret_cast<IUnknown*>(ShaderResourceView);
	ReleaseIfValid(Resource);
	ShaderResourceView = nullptr;

	Resource = reinterpret_cast<IUnknown*>(RenderTargetView);
	ReleaseIfValid(Resource);
	RenderTargetView = nullptr;

	Resource = reinterpret_cast<IUnknown*>(RenderTargetTexture);
	ReleaseIfValid(Resource);
	RenderTargetTexture = nullptr;

	OffscreenWidth = 0;
	OffscreenHeight = 0;
}

bool CViewport::GetMousePositionInViewport(int32 WindowMouseX, int32 WindowMouseY, int32& OutViewportX, int32& OutViewportY, int32& OutWidth, int32& OutHeight) const
{
	if (!bVisible || OffscreenWidth == 0 || OffscreenHeight == 0)
	{
		return false;
	}

	if (WindowMouseX < ClientPosX || WindowMouseY < ClientPosY)
	{
		return false;
	}

	const int32 LocalX = WindowMouseX - ClientPosX;
	const int32 LocalY = WindowMouseY - ClientPosY;
	if (LocalX < 0 || LocalY < 0 || LocalX >= static_cast<int32>(OffscreenWidth) || LocalY >= static_cast<int32>(OffscreenHeight))
	{
		return false;
	}

	OutViewportX = LocalX;
	OutViewportY = LocalY;
	OutWidth = static_cast<int32>(OffscreenWidth);
	OutHeight = static_cast<int32>(OffscreenHeight);
	return true;
}

void CViewport::ReadySceneView(ID3D11Device* Device, uint32 Width, uint32 Height)
{
	if (Device == nullptr)
	{
		return;
	}

	if (Width == 0 || Height == 0)
	{
		ReleaseSceneView();
		return;
	}

	if (RenderTargetView && ShaderResourceView && DepthStencilView &&
		OffscreenWidth == Width && OffscreenHeight == Height)
	{
		return;
	}

	ReleaseSceneView();

	D3D11_TEXTURE2D_DESC ColorDesc = {};
	ColorDesc.Width = Width;
	ColorDesc.Height = Height;
	ColorDesc.MipLevels = 1;
	ColorDesc.ArraySize = 1;
	ColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ColorDesc.SampleDesc.Count = 1;
	ColorDesc.Usage = D3D11_USAGE_DEFAULT;
	ColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	if (FAILED(Device->CreateTexture2D(&ColorDesc, nullptr, &RenderTargetTexture)))
	{
		ReleaseSceneView();
		return;
	}

	if (FAILED(Device->CreateRenderTargetView(RenderTargetTexture, nullptr, &RenderTargetView)))
	{
		ReleaseSceneView();
		return;
	}

	if (FAILED(Device->CreateShaderResourceView(RenderTargetTexture, nullptr, &ShaderResourceView)))
	{
		ReleaseSceneView();
		return;
	}

	D3D11_TEXTURE2D_DESC DepthDesc = {};
	DepthDesc.Width = Width;
	DepthDesc.Height = Height;
	DepthDesc.MipLevels = 1;
	DepthDesc.ArraySize = 1;
	DepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DepthDesc.SampleDesc.Count = 1;
	DepthDesc.Usage = D3D11_USAGE_DEFAULT;
	DepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

	if (FAILED(Device->CreateTexture2D(&DepthDesc, nullptr, &DepthStencilTexture)))
	{
		ReleaseSceneView();
		return;
	}

	if (FAILED(Device->CreateDepthStencilView(DepthStencilTexture, nullptr, &DepthStencilView)))
	{
		ReleaseSceneView();
		return;
	}

	OffscreenWidth = Width;
	OffscreenHeight = Height;
}
