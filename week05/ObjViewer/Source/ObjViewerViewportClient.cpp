#include "ObjViewerViewportClient.h"

#include <shellapi.h>
#include <string>

#include "ObjViewerEngine.h"
#include "ObjViewerShell.h"

#include "Camera/Camera.h"
#include "Component/CameraComponent.h"
#include "Core/Engine.h"
#include "Input/InputManager.h"
#include "Renderer/Renderer.h"
#include "World/World.h"

namespace
{
	bool HasObjExtension(const wchar_t* FilePath)
	{
		if (FilePath == nullptr)
		{
			return false;
		}

		const wchar_t* Extension = wcsrchr(FilePath, L'.');
		if (Extension == nullptr)
		{
			return false;
		}

		return _wcsicmp(Extension, L".obj") == 0;
	}

	FString WideToUtf8(const wchar_t* WideString)
	{
		if (WideString == nullptr || WideString[0] == L'\0')
		{
			return "";
		}

		const int32 RequiredBytes = ::WideCharToMultiByte(CP_UTF8, 0, WideString, -1, nullptr, 0, nullptr, nullptr);
		if (RequiredBytes <= 1)
		{
			return "";
		}

		FString Result;
		Result.resize(static_cast<size_t>(RequiredBytes));
		::WideCharToMultiByte(CP_UTF8, 0, WideString, -1, Result.data(), RequiredBytes, nullptr, nullptr);
		Result.pop_back();
		return Result;
	}
}

void FObjViewerViewportClient::Attach(FEngine* Engine, FRenderer* Renderer)
{
	FGameViewportClient::Attach(Engine, Renderer);

	if (Engine)
	{
		if (FObjViewerEngine* ViewerEngine = static_cast<FObjViewerEngine*>(Engine))
		{
			ViewerEngine->GetShell().AttachToRenderer(Renderer);
		}
	}

	if (Renderer && Renderer->GetHwnd())
	{
		::DragAcceptFiles(Renderer->GetHwnd(), TRUE);
	}
}

void FObjViewerViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
	if (Renderer && Renderer->GetHwnd())
	{
		::DragAcceptFiles(Renderer->GetHwnd(), FALSE);
	}

	if (Engine)
	{
		if (FObjViewerEngine* ViewerEngine = static_cast<FObjViewerEngine*>(Engine))
		{
			ViewerEngine->GetShell().DetachFromRenderer(Renderer);
		}
	}

	FGameViewportClient::Detach(Engine, Renderer);
}

void FObjViewerViewportClient::Tick(FEngine* Engine, float DeltaTime)
{
	FGameViewportClient::Tick(Engine, DeltaTime);

	FObjViewerEngine* ViewerEngine = static_cast<FObjViewerEngine*>(Engine);
	if (ViewerEngine == nullptr)
	{
		return;
	}

	FInputManager* Input = ViewerEngine->GetInputManager();
	UWorld* ViewerWorld = ViewerEngine->GetActiveWorld();
	if (Input == nullptr || ViewerWorld == nullptr)
	{
		return;
	}

	UCameraComponent* ActiveCamera = ViewerWorld->GetActiveCameraComponent();
	if (ActiveCamera == nullptr)
	{
		return;
	}

	FCamera* Camera = ActiveCamera->GetCamera();
	if (Camera == nullptr)
	{
		return;
	}

	FObjViewerShell& Shell = ViewerEngine->GetShell();
	const bool bRightMouseDown = Input->IsMouseButtonDown(FInputManager::MOUSE_RIGHT);
	if (!bRightMouseDown)
	{
		return;
	}

	if (Shell.WantsViewportMouseInput())
	{
		const float Sensitivity = Camera->GetMouseSensitivity();
		ActiveCamera->Rotate(
			Input->GetMouseDeltaX() * Sensitivity,
			-Input->GetMouseDeltaY() * Sensitivity);
	}

	if (!Shell.WantsViewportKeyboardInput())
	{
		return;
	}

	if (Input->IsKeyDown('W'))
	{
		ActiveCamera->MoveForward(DeltaTime);
	}
	if (Input->IsKeyDown('S'))
	{
		ActiveCamera->MoveForward(-DeltaTime);
	}
	if (Input->IsKeyDown('D'))
	{
		ActiveCamera->MoveRight(DeltaTime);
	}
	if (Input->IsKeyDown('A'))
	{
		ActiveCamera->MoveRight(-DeltaTime);
	}
	if (Input->IsKeyDown('E'))
	{
		ActiveCamera->MoveUp(DeltaTime);
	}
	if (Input->IsKeyDown('Q'))
	{
		ActiveCamera->MoveUp(-DeltaTime);
	}
}

void FObjViewerViewportClient::HandleMessage(FEngine* Engine, HWND Hwnd, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	FGameViewportClient::HandleMessage(Engine, Hwnd, Msg, WParam, LParam);

	if (Msg != WM_DROPFILES)
	{
		return;
	}

	HDROP DropHandle = reinterpret_cast<HDROP>(WParam);
	if (!DropHandle)
	{
		return;
	}

	wchar_t FilePathBuffer[MAX_PATH] = {};
	const UINT FileCount = ::DragQueryFileW(DropHandle, 0xFFFFFFFF, nullptr, 0);
	if (FileCount > 0)
	{
		::DragQueryFileW(DropHandle, 0, FilePathBuffer, MAX_PATH);
		if (HasObjExtension(FilePathBuffer))
		{
			LoadDroppedObj(static_cast<FObjViewerEngine*>(Engine), WideToUtf8(FilePathBuffer));
		}
	}

	::DragFinish(DropHandle);
}

void FObjViewerViewportClient::LoadDroppedObj(FObjViewerEngine* Engine, const FString& FilePath)
{
	if (!Engine)
	{
		return;
	}

	Engine->GetShell().RequestImportDialog(FilePath, "Drag & Drop");
}
