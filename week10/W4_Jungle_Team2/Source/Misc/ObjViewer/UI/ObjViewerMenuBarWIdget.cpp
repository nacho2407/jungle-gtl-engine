#include "ObjViewerMenuBarWidget.h"

#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Misc/ObjViewer/Settings/ObjViewerSettings.h"
#include "Engine/Core/Paths.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "Core/ResourceManager.h"
#include "Component/StaticMeshComponent.h"
#include "Math/Rotator.h"
#include "ImGui/imgui.h"
#include "Engine/Core/Paths.h"

#include <windows.h>
#include <commdlg.h>
#include <filesystem>

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
					UStaticMesh* LoadedMesh = FResourceManager::Get().LoadStaticMesh(FilePath);	
					if (LoadedMesh)
					{
						if (UStaticMeshComponent* TargetComponent = Engine->GetPreviewMeshComponent()) 
						{
							TargetComponent->SetStaticMesh(LoadedMesh);

							if (FObjViewerSettings::Get().ModelUpAxis == EModelUpAxis::Y_up)
							{
								TargetComponent->GetOwner()->SetActorRotation(FVector(90.0f, 0.0f, 0.0f));
							}
							else
							{
								TargetComponent->GetOwner()->SetActorRotation(FVector(0.0f, 0.0f, 0.0f));
							}
							
							Engine->GetViewportClient().ResetCamera();
						}
					}
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

// 불러올 때는 상대 경로로 불러온다.
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

	ShowCursor(TRUE);  // 엔진 내부의 커서 숨김 초기화

	if (GetOpenFileNameW(&ofn) == TRUE)
	{
		return FPaths::ToRelativeString(ofn.lpstrFile);
	}
	return FString();
}

// 저장할 때는 절대경로로 저장한다.
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
		return FPaths::ToAbsoluteString(ofn.lpstrFile);
	}
	return FString();
}