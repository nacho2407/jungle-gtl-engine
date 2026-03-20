#include "Editor/UI/EditorMainPanel.h"

#include "Editor/EditorEngine.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_impl_dx11.h"
#include "ImGui/imgui_impl_win32.h"

#include "Render/Renderer/Renderer.h"
#include "Component/PrimitiveComponent.h"
#include "Serialization/SceneSaveManager.h"
#include "Core/Common.h"
#include "Engine/Core/InputSystem.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorMainPanel::Create(HWND InHWindow, FRenderer& InRenderer, FEditorEngine* InEditorEngine)
{
	EditorEngine = InEditorEngine;
	SelectedPrimitiveType = static_cast<int>(EPrimitiveType::EPT_Cube);


	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init((void*)InHWindow);
	ImGui_ImplDX11_Init(InRenderer.GetFD3DDevice().GetDevice(), InRenderer.GetFD3DDevice().GetDeviceContext());
}

void FEditorMainPanel::Release()
{
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void FEditorMainPanel::Render(float DeltaTime, FViewOutput& ViewOutput)
{
	using namespace common::constants::ImGui;
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();

	ImGui::NewFrame();

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Control Panel");

	ConsoleInstance.Draw("Console", &bShowConsole);

	ImGui::Text("FPS : %.1f", EditorEngine->GetMainLoopFPS());

	ImGui::SameLine();

	ImGui::Text("Memory Allocated : %d", EngineStatics::GetTotalAllocationBytes());

	ImGui::SameLine();

	ImGui::Text("Times Allocated : %d", EngineStatics::GetTotalAllocationCount());

	SEPARATOR();

	ImGui::Combo("Primitive", &SelectedPrimitiveType, PrimitiveTypes, IM_ARRAYSIZE(PrimitiveTypes));

	if (ImGui::Button("Spawn"))
	{
		for (int i = 0; i < NumberOfSpawnedActors; i++) {
			switch ((EPrimitiveType)SelectedPrimitiveType)
			{
			case EPrimitiveType::EPT_Cube:
				EditorEngine->SpawnNewPrimitiveActor<UCubeComponent>(CurSpawnPoint);
				break;
			case EPrimitiveType::EPT_Sphere:
				EditorEngine->SpawnNewPrimitiveActor<USphereComponent>(CurSpawnPoint);
				break;
			case EPrimitiveType::EPT_Plane:
				EditorEngine->SpawnNewPrimitiveActor<UPlaneComponent>(CurSpawnPoint);
				break;
			}
		}

		NumberOfSpawnedActors = 1;
	}

	ImGui::InputInt("Number of Spawn", &NumberOfSpawnedActors, 1, 10);

	SEPARATOR();

	if (ImGui::Button("New Scene")) {
		EditorEngine->GetGizmo()->SetVisibility(false);
		ViewOutput.Object = nullptr;
		EditorEngine->NewScene();
		NewSceneNotificationTimer = NotificationTimer;
	}
	if (NewSceneNotificationTimer > 0.0f) {
		NewSceneNotificationTimer -= DeltaTime;
		ImGui::SameLine();
		ImGui::Text("New scene created");
	}

	if (ImGui::Button("Save Scene")) {
		FSceneSaveManager::SaveSceneAsJSON(SceneName, EditorEngine->GetScene());
		SceneSaveNotificationTimer = NotificationTimer;
	}
	if (SceneSaveNotificationTimer > 0.0f) {
		SceneSaveNotificationTimer -= DeltaTime;
		ImGui::Text("Scene saved");
	}
	ImGui::SameLine();
	ImGui::InputText("Scene Name", SceneName, IM_ARRAYSIZE(SceneName));

	if (ImGui::Button("Load Scene")) {
		if (std::ifstream(LoadPath).is_open()) {
			EditorEngine->GetGizmo()->SetVisibility(false);
			EditorEngine->ClearScene();
			ViewOutput.Object = nullptr;
			FSceneSaveManager::LoadSceneFromJSON(LoadPath, EditorEngine->GetScene());
			EditorEngine->ResetViewport();
			Sleep(50);
			SceneLoadNotificationTimer = NotificationTimer;
		}
	}
	if (SceneLoadNotificationTimer > 0.0f) {
		SceneLoadNotificationTimer -= DeltaTime;
		ImGui::Text("Scene loaded");
	}

	ImGui::SameLine();
	ImGui::InputText("Load Path", LoadPath, IM_ARRAYSIZE(LoadPath));

	SEPARATOR();
	FCameraState& CameraState = EditorEngine->GetCameraState();
	ImGui::Checkbox("Orthographic", &(CameraState.bIsOrthogonal));

	float CameraFOV_Deg = CameraState.FOV * (RAD_TO_DEG);

	if (ImGui::DragFloat("Camera FOV",
		&CameraFOV_Deg,
		0.5f,          // speed in degrees
		1.0f,          // min deg
		90.0f))        // max deg
	{
		CameraState.FOV = CameraFOV_Deg * (DEG_TO_RAD);
	}

	float OrthoWidth = CameraState.OrthoWidth;
	if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
	{
		CameraState.OrthoWidth = Clamp(OrthoWidth, 0.1f, 1000.0f);
	}
	UCamera* Camera = EditorEngine->GetCamera();
	FVector CamPos = Camera->GetWorldLocation();
	float CameraLocation[3] = { CamPos.X, CamPos.Y, CamPos.Z };
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f))
	{
		Camera->SetWorldLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
		
	}
	FVector CamRot = Camera->GetRelativeRotation();
	float CameraRotation[3] = { CamRot.X, CamRot.Y, CamRot.Z };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
	{
		Camera->SetRelativeRotation(FVector(Clamp(CameraRotation[0], CamRot.X, CamRot.X), CameraRotation[1], CameraRotation[2]));
	}

	SEPARATOR();

	// Space Select Button(World or Local)
	static int SelectedSpace = 0;
	if (ImGui::RadioButton("World", &SelectedSpace, 0))
	{
		EditorEngine->GetEditorGizmo()->SetWorldSpace(true);
		std::cout << "Switched to World Space\n";
	}

	ImGui::SameLine();

	if (ImGui::RadioButton("Local", &SelectedSpace, 1))
	{
		EditorEngine->GetEditorGizmo()->SetWorldSpace(false);
		std::cout << "Switched to Local Space\n";
	}


	SEPARATOR();

	if (ImGui::Button("Translate")) EditorEngine->GetEditorGizmo()->SetTranslateMode();
	ImGui::SameLine();
	if (ImGui::Button("Rotate")) EditorEngine->GetEditorGizmo()->SetRotateMode();
	ImGui::SameLine();
	if (ImGui::Button("Scale")) EditorEngine->GetEditorGizmo()->SetScaleMode();


	SEPARATOR();

	if (ImGui::Button(EditorEngine->GetRenderHandler().bGridVisible ? "Grid : OFF" : "Grid : ON"))
	{
		EditorEngine->GetRenderHandler().bGridVisible = !EditorEngine->GetRenderHandler().bGridVisible;
	}


	ImGui::End();

	RenderObjectWindow(ViewOutput.Object);

	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void FEditorMainPanel::RenderObjectWindow(UObject*& ObjectPicked) {
	ImGui::Begin("Picked Object");
	if(!ObjectPicked) {
		ImGui::End();
		return;
	}
	if (ObjectPicked) {
		ImGui::Text("Class: %s", ObjectPicked->GetTypeInfo()->name);
		ImGui::Text("Object Size: %d", sizeof(*ObjectPicked));
	}
	if (ObjectPicked->IsA<USceneComponent>()) {
		ImGui::Text("Transform");
		ImGui::Separator();

		USceneComponent* SceneComp = ObjectPicked->Cast<USceneComponent>();
		FVector Pos = SceneComp->GetWorldLocation();
		float PosArray[3] = { Pos.X, Pos.Y, Pos.Z };

		FVector Rot = SceneComp->GetRelativeRotation();
		float RotArray[3] = { Rot.X, Rot.Y, Rot.Z };

		FVector Scale = SceneComp->GetRelativeScale();
		float ScaleArray[3] = { Scale.X, Scale.Y, Scale.Z };


		UGizmoComponent* Gizmo = EditorEngine->GetGizmo();
		if (ImGui::DragFloat3("Location", PosArray, 0.1f))
		{
			Gizmo->SetTargetLocation(FVector(PosArray[0], PosArray[1], PosArray[2]));
		}
		if (ImGui::DragFloat3("Rotation", RotArray, 0.1f))
		{
			Gizmo->SetTargetRotation(FVector(RotArray[0], RotArray[1], RotArray[2]));
		}
		if (ImGui::DragFloat3("Scale", ScaleArray, 0.1f))
		{
			Gizmo->SetTargetScale(FVector(ScaleArray[0], ScaleArray[1], ScaleArray[2]));
		}

		SEPARATOR();

		if (ImGui::Button("Remove Object") && ObjectPicked) {
			ObjectPicked->bPendingKill = true;
			if (ObjectPicked->IsA<USceneComponent>()) {
				USceneComponent* SceneComp = ObjectPicked->Cast<USceneComponent>();
				if (SceneComp->GetOwner()) {
					// TODO:: Do this recursively
					UObjectManager::Get().DestroyObject(SceneComp->GetOwner());
				}
			}
			EditorEngine->GetGizmo()->SetVisibility(false);
			EditorEngine->GetGizmo()->Deactivate();
			ObjectPicked = nullptr;
		}
	}

	ImGui::End();
}

void FEditorMainPanel::Update()
{
	ImGuiIO& io = ImGui::GetIO();

	InputSystem::GuiInputState.bUsingMouse = io.WantCaptureMouse; 
	InputSystem::GuiInputState.bUsingKeyboard = io.WantCaptureKeyboard;
}
