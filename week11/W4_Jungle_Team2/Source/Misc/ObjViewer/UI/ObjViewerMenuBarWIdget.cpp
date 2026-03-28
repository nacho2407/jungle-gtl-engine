#include "ObjViewerMenuBarWidget.h"
#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Engine/Core/Paths.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "ImGui/imgui.h"

#include <windows.h>
#include <commdlg.h>

void FObjViewerMenuBarWidget::Render(float DeltaTime)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Files"))
		{
			if (ImGui::MenuItem("Load..."))
			{
				FString FilePath = OpenFileDialog();
				if (!FilePath.empty())
				{
					// TODO: 모델 로드 로직
				}
			}
			if (ImGui::MenuItem("Save"))
			{
				// TODO: 덮어쓰기 로직
			}
			if (ImGui::MenuItem("Save As..."))
			{
				FString FilePath = SaveFileDialog();
				if (!FilePath.empty())
				{
					// TODO: 다른 이름으로 저장 로직
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

FString FObjViewerMenuBarWidget::OpenFileDialog()
{
	OPENFILENAMEW ofn;
	WCHAR szFile[MAX_PATH] = { 0 };

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = static_cast<HWND>(Engine->GetWindow()->GetHWND());
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
	ofn.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (GetOpenFileNameW(&ofn) == TRUE)
	{
		return FPaths::ToUtf8(ofn.lpstrFile);
	}
	return FString();
}

FString FObjViewerMenuBarWidget::SaveFileDialog()
{
	OPENFILENAMEW ofn;
	WCHAR szFile[MAX_PATH] = { 0 };

	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = static_cast<HWND>(Engine->GetWindow()->GetHWND());
	ofn.lpstrFile = szFile;
	ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
	ofn.lpstrFilter = L"OBJ Files (*.obj)\0*.obj\0";
	ofn.nFilterIndex = 1;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (GetSaveFileNameW(&ofn) == TRUE)
	{
		return FPaths::ToUtf8(ofn.lpstrFile);
	}
	return FString();
}