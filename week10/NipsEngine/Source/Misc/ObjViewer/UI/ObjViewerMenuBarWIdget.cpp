#include "ObjViewerMenuBarWidget.h"

#include "Runtime/WindowsWindow.h"
#include "Core/Paths.h"
#include "Core/ResourceManager.h"

#include "Misc/ObjViewer/ObjViewerEngine.h"
#include "Misc/ObjViewer/Settings/ObjViewerSettings.h"
#include "Viewport/ViewportCamera.h"
#include "Component/StaticMeshComponent.h"

#include "Serialization/SceneSaveManager.h"
#include "Math/Rotator.h"

#include "ImGui/imgui.h"
#include <windows.h>
#include <commdlg.h>
#include <filesystem>

void FObjViewerMenuBarWidget::Render(float DeltaTime)
{
	if (ImGui::BeginMainMenuBar())
	{
		if (ImGui::BeginMenu("Files"))
		{
			if (ImGui::MenuItem("Load Obj..."))
				LoadObj();

			// if (ImGui::MenuItem("Load Scene..."))
			//  	LoadScene();

			ImGui::EndMenu();
		}
		ImGui::EndMainMenuBar();
	}
}

void FObjViewerMenuBarWidget::LoadObj()
{
	FString FilePath = OpenFileDialog(L"OBJ Files (*.obj)\0*.obj\0All Files\0*.*\0");

	if (!FilePath.empty())
	{
		UStaticMesh* LoadedMesh = FResourceManager::Get().LoadStaticMesh(FilePath);

		if (!LoadedMesh) return;

		UStaticMeshComponent* TargetComponent = Engine->GetPreviewMeshComponent();

		if (!TargetComponent) return; 

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

void FObjViewerMenuBarWidget::LoadScene()
{
	FString FilePath = OpenFileDialog(L"Scene Files (*.Scene; *.json)\0*.Scene;*.json\0All Files\0*.*\0");
    if (FilePath.empty()) return;

	FWorldContext LoadCtx;
	FEditorCameraState LoadedCam;
	FSceneSaveManager::LoadSceneFromJSON(FilePath, LoadCtx, &LoadedCam);
	
	if (LoadCtx.World)
	{
		Engine->GetWorldList().clear();
		Engine->GetWorldList().push_back(LoadCtx);
		Engine->SetActiveWorld(LoadCtx.ContextHandle);
	}
	
	Engine->GetViewportClient().ResetCamera();

	// 로드된 카메라 상태가 유효하다면 적용합니다
	if (LoadedCam.bValid)
	{
		if (FViewportCamera* Cam = Engine->GetCamera())
		{
			Cam->SetLocation(LoadedCam.Location);
			Cam->SetRotation(FQuat(LoadedCam.Rotation));
			Cam->SetFOV(LoadedCam.FOV);
			Cam->SetNearPlane(LoadedCam.NearClip);
			Cam->SetFarPlane(LoadedCam.FarClip);
		}
	}
}

FString FObjViewerMenuBarWidget::OpenFileDialog(const wchar_t* Filter)
{
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(Engine->GetWindow()->GetHWND());
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(WCHAR);
    ofn.lpstrFilter = Filter; // 매개변수 적용
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

    ShowCursor(TRUE);  // 엔진 내부의 커서 숨김 초기화

    if (GetOpenFileNameW(&ofn) == TRUE)
    {
        return FPaths::ToRelativeString(ofn.lpstrFile);
    }
    return FString();
}

FString FObjViewerMenuBarWidget::SaveFileDialog(const wchar_t* Filter, const wchar_t* DefExt)
{
    OPENFILENAMEW ofn;
    wchar_t szFile[MAX_PATH] = { 0 };

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(Engine->GetWindow()->GetHWND());
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile) / sizeof(wchar_t);
    ofn.lpstrFilter = Filter; 
    ofn.lpstrDefExt = DefExt;
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

    if (GetSaveFileNameW(&ofn) == TRUE)
    {
        return FPaths::ToAbsoluteString(ofn.lpstrFile);
    }
    return FString();
}