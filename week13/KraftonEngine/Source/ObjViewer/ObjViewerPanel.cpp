#include "ObjViewer/ObjViewerPanel.h"

#include "ObjViewer/ObjViewerEngine.h"
#include "ObjViewer/ObjViewerViewportClient.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Engine/Input/InputSystem.h"
#include "Render/Pipeline/Renderer.h"
#include "Mesh/ObjManager.h"
#include "Viewport/Viewport.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

void FObjViewerPanel::Create(FWindowsWindow* InWindow, FRenderer& InRenderer, UObjViewerEngine* InEngine)
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& IO = ImGui::GetIO();
	IO.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	Window = InWindow;
	Engine = InEngine;

	IO.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\malgun.ttf", 16.0f, nullptr, IO.Fonts->GetGlyphRangesKorean());

	ImGui_ImplWin32_Init((void*)InWindow->GetHWND());
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());
}

void FObjViewerPanel::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FObjViewerPanel::Render(float DeltaTime)
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

	RenderMeshList();
	RenderImportPopup();
	RenderPreviewViewport(DeltaTime);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FObjViewerPanel::Update()
{
	ImGuiIO& IO = ImGui::GetIO();

	// 프리뷰 뷰포트 위에서는 ImGui 마우스 캡처 해제
	bool bWantMouse = IO.WantCaptureMouse;
	FObjViewerViewportClient* VC = Engine ? Engine->GetViewportClient() : nullptr;
	if (VC)
	{
		// ImGui hover 체크를 통해 뷰포트 영역 감지
		// (뷰포트 ImGui::Image 위에 InvisibleButton이 있으므로 그걸로 판단)
		bWantMouse = IO.WantCaptureMouse;
	}
	InputSystem::Get().GetGuiInputState().bUsingMouse = bWantMouse;
	InputSystem::Get().GetGuiInputState().bUsingKeyboard = IO.WantCaptureKeyboard;
	InputSystem::Get().GetGuiInputState().bUsingTextInput = IO.WantTextInput;
}

void FObjViewerPanel::RenderMeshList()
{
	ImGui::Begin("Mesh List");

	// OBJ Files 섹션
	if (ImGui::CollapsingHeader("OBJ Files", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::GetAvailableObjFiles();

		for (int32 i = 0; i < static_cast<int32>(ObjFiles.size()); ++i)
		{
			bool bSelected = (i == SelectedObjIndex);
			if (ImGui::Selectable(ObjFiles[i].DisplayName.c_str(), bSelected))
			{
				SelectedObjIndex = i;
			}
		}

		// Import 버튼 — 선택된 OBJ가 있을 때만 활성
		bool bHasSelection = SelectedObjIndex >= 0 && SelectedObjIndex < static_cast<int32>(ObjFiles.size());
		if (!bHasSelection) ImGui::BeginDisabled();
		if (ImGui::Button("Import..."))
		{
			PendingImportOptions = FImportOptions::Default();
			bShowImportPopup = true;
			ImGui::OpenPopup("Import Options");
		}
		if (!bHasSelection) ImGui::EndDisabled();
	}

	ImGui::Separator();

	// Cached Meshes (.bin) 섹션
	if (ImGui::CollapsingHeader("Cached Meshes (.bin)", ImGuiTreeNodeFlags_DefaultOpen))
	{
		const TArray<FMeshAssetListItem>& MeshFiles = FObjManager::GetAvailableMeshFiles();

		for (int32 i = 0; i < static_cast<int32>(MeshFiles.size()); ++i)
		{
			bool bSelected = (i == SelectedMeshIndex);
			if (ImGui::Selectable(MeshFiles[i].DisplayName.c_str(), bSelected))
			{
				if (SelectedMeshIndex != i)
				{
					SelectedMeshIndex = i;
					if (Engine)
					{
						Engine->LoadPreviewMesh(MeshFiles[i].FullPath);
					}
				}
			}
		}
	}

	ImGui::End();
}

void FObjViewerPanel::RenderImportPopup()
{
	if (!bShowImportPopup) return;

	ImGui::OpenPopup("Import Options");

	ImVec2 Center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(Center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	if (ImGui::BeginPopupModal("Import Options", &bShowImportPopup, ImGuiWindowFlags_AlwaysAutoResize))
	{
		// Scale
		ImGui::InputFloat("Scale", &PendingImportOptions.Scale, 0.01f, 1.0f, "%.4f");
		ImGui::SameLine();
		if (ImGui::SmallButton("cm->m"))
		{
			PendingImportOptions.Scale = 0.01f;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("1:1"))
		{
			PendingImportOptions.Scale = 1.0f;
		}

		// Forward Axis
		const char* AxisLabels[] = { "X", "-X", "Y", "-Y", "Z", "-Z" };
		int AxisIndex = static_cast<int>(PendingImportOptions.ForwardAxis);
		if (ImGui::Combo("Forward Axis", &AxisIndex, AxisLabels, IM_ARRAYSIZE(AxisLabels)))
		{
			PendingImportOptions.ForwardAxis = static_cast<EForwardAxis>(AxisIndex);
		}

		// Winding Order
		const char* WindingLabels[] = { "CCW -> CW (DirectX)", "Keep Original" };
		int WindingIndex = static_cast<int>(PendingImportOptions.WindingOrder);
		if (ImGui::Combo("Winding Order", &WindingIndex, WindingLabels, IM_ARRAYSIZE(WindingLabels)))
		{
			PendingImportOptions.WindingOrder = static_cast<EWindingOrder>(WindingIndex);
		}

		ImGui::Separator();

		// Import / Cancel 버튼
		if (ImGui::Button("Import", ImVec2(120, 0)))
		{
			const TArray<FMeshAssetListItem>& ObjFiles = FObjManager::GetAvailableObjFiles();
			if (Engine && SelectedObjIndex >= 0 && SelectedObjIndex < static_cast<int32>(ObjFiles.size()))
			{
				Engine->ImportObjWithOptions(ObjFiles[SelectedObjIndex].FullPath, PendingImportOptions);
			}
			bShowImportPopup = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();

		if (ImGui::Button("Cancel", ImVec2(120, 0)))
		{
			bShowImportPopup = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void FObjViewerPanel::RenderPreviewViewport(float DeltaTime)
{
	ImGui::Begin("Preview");

	FObjViewerViewportClient* VC = Engine ? Engine->GetViewportClient() : nullptr;

	if (VC)
	{
		ImVec2 Pos = ImGui::GetCursorScreenPos();
		ImVec2 Size = ImGui::GetContentRegionAvail();

		if (Size.x > 0 && Size.y > 0)
		{
			VC->SetViewportRect(Pos.x, Pos.y, Size.x, Size.y);
			VC->RenderViewportImage();

			// 투명 버튼으로 ImGui 마우스 캡처를 뷰포트 위에서 해제
			ImGui::InvisibleButton("##PreviewViewport", Size);
			if (ImGui::IsItemHovered())
			{
				InputSystem::Get().GetGuiInputState().bUsingMouse = false;
			}
		}
	}

	ImGui::End();
}
