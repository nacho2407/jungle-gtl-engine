//#include "ObjViewer/ObjViewerViewportClient.h"
//
//#include "Engine/Input/InputSystem.h"
//#include "Engine/Runtime/WindowsWindow.h"
//#include "Component/CameraComponent.h"
//#include "Viewport/Viewport.h"
//#include "Math/MathUtils.h"
//#include "ImGui/imgui.h"
//
//#include <cmath>
//
//void FObjViewerViewportClient::Initialize(FWindowsWindow* InWindow)
//{
//	Window = InWindow;
//}
//
//void FObjViewerViewportClient::Release()
//{
//	DestroyCamera();
//	if (Viewport)
//	{
//		Viewport->Release();
//		delete Viewport;
//		Viewport = nullptr;
//	}
//}
//
//void FObjViewerViewportClient::CreateCamera()
//{
//	DestroyCamera();
//	Camera = UObjectManager::Get().CreateObject<UCameraComponent>();
//}
//
//void FObjViewerViewportClient::DestroyCamera()
//{
//	if (Camera)
//	{
//		UObjectManager::Get().DestroyObject(Camera);
//		Camera = nullptr;
//	}
//}
//
//void FObjViewerViewportClient::ResetCamera()
//{
//	OrbitTarget = FVector(0, 0, 0);
//	OrbitDistance = 5.0f;
//	OrbitYaw = 0.0f;
//	OrbitPitch = 30.0f;
//}
//
//static void UpdateOrbitCamera(UCameraComponent* Camera, const FVector& Target, float Distance, float Yaw, float Pitch)
//{
//	float YawRad = Yaw * DEG_TO_RAD;
//	float PitchRad = Pitch * DEG_TO_RAD;
//
//	float CosPitch = cosf(PitchRad);
//	FVector Offset;
//	Offset.X = Distance * CosPitch * cosf(YawRad);
//	Offset.Y = Distance * CosPitch * sinf(YawRad);
//	Offset.Z = Distance * sinf(PitchRad);
//
//	Camera->SetWorldLocation(Target + Offset);
//	Camera->LookAt(Target);
//}
//
//void FObjViewerViewportClient::Tick(float DeltaTime)
//{
//	TickInput(DeltaTime);
//
//	if (Camera)
//	{
//		UpdateOrbitCamera(Camera, OrbitTarget, OrbitDistance, OrbitYaw, OrbitPitch);
//	}
//}
//
//void FObjViewerViewportClient::TickInput(float DeltaTime)
//{
//	if (!Camera) return;
//	if (InputSystem::Get().GetGuiInputState().bUsingKeyboard) return;
//
//	// 마우스가 뷰포트 영역 안에 있는지 확인
//	POINT MousePos = InputSystem::Get().GetMousePos();
//	if (Window)
//	{
//		MousePos = Window->ScreenToClientPoint(MousePos);
//	}
//	float MX = static_cast<float>(MousePos.x);
//	float MY = static_cast<float>(MousePos.y);
//
//	bool bMouseInViewport = (MX >= ViewportX && MX <= ViewportX + ViewportWidth &&
//		MY >= ViewportY && MY <= ViewportY + ViewportHeight);
//
//	if (!bMouseInViewport) return;
//
//	// 우클릭 드래그 → 오빗 회전
//	if (InputSystem::Get().GetKey(VK_RBUTTON))
//	{
//		float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
//		float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());
//
//		OrbitYaw += DeltaX * 0.3f;
//		OrbitPitch += DeltaY * 0.3f;
//		OrbitPitch = Clamp(OrbitPitch, -89.0f, 89.0f);
//	}
//
//	// 중클릭 드래그 → 팬
//	if (InputSystem::Get().GetKey(VK_MBUTTON))
//	{
//		float DeltaX = static_cast<float>(InputSystem::Get().MouseDeltaX());
//		float DeltaY = static_cast<float>(InputSystem::Get().MouseDeltaY());
//
//		float PanScale = OrbitDistance * 0.002f;
//		FVector Right = Camera->GetRightVector();
//		FVector Up = Camera->GetUpVector();
//		OrbitTarget = OrbitTarget - Right * (DeltaX * PanScale) + Up * (DeltaY * PanScale);
//	}
//
//	// 스크롤 → 줌
//	float ScrollNotches = InputSystem::Get().GetScrollNotches();
//	if (ScrollNotches != 0.0f)
//	{
//		OrbitDistance -= ScrollNotches * OrbitDistance * 0.1f;
//		OrbitDistance = Clamp(OrbitDistance, 0.1f, 500.0f);
//	}
//}
//
//void FObjViewerViewportClient::SetViewportRect(float X, float Y, float Width, float Height)
//{
//	ViewportX = X;
//	ViewportY = Y;
//	ViewportWidth = Width;
//	ViewportHeight = Height;
//
//	// FViewport 리사이즈 요청
//	if (Viewport)
//	{
//		uint32 W = static_cast<uint32>(Width);
//		uint32 H = static_cast<uint32>(Height);
//		if (W > 0 && H > 0 && (W != Viewport->GetWidth() || H != Viewport->GetHeight()))
//		{
//			Viewport->RequestResize(W, H);
//		}
//	}
//}
//
//void FObjViewerViewportClient::RenderViewportImage()
//{
//	if (!Viewport || !Viewport->GetSRV()) return;
//	if (ViewportWidth <= 0 || ViewportHeight <= 0) return;
//
//	ImDrawList* DrawList = ImGui::GetWindowDrawList();
//	ImVec2 Min(ViewportX, ViewportY);
//	ImVec2 Max(ViewportX + ViewportWidth, ViewportY + ViewportHeight);
//
//	DrawList->AddImage((ImTextureID)Viewport->GetSRV(), Min, Max);
//}
