#include "Editor/UI/EditorControlWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/Viewport/ViewportCamera.h"
#include "Core/Logging/Timer.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ResourceManager.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorControlWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	SelectedPrimitiveType = 0;
}

void FEditorControlWidget::Render(float DeltaTime)
{
	(void)DeltaTime;
	if (!EditorEngine)
	{
		return;
	}

	ImGui::SetNextWindowCollapsed(false, ImGuiCond_Once);
	ImGui::SetNextWindowSize(ImVec2(500.0f, 480.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Control Panel");

	// Spawn
	ImGui::Combo("Primitive", &SelectedPrimitiveType, PrimitiveTypes, IM_ARRAYSIZE(PrimitiveTypes));

	if (ImGui::Button("Spawn"))
	{
		UWorld* World = EditorEngine->GetWorld();
		if (!World)
		{
			ImGui::End();
			return;
		}

		auto SpawnStaticMeshActor = [&]()
		{
			AActor* Actor = World->SpawnActor<AActor>();
			Actor->SetActorLocation(CurSpawnPoint);

			UStaticMeshComponent* StaticMeshComp = Actor->AddComponent<UStaticMeshComponent>();
			Actor->SetRootComponent(StaticMeshComp);
		};

		for (int32 i = 0; i < NumberOfSpawnedActors; i++)
		{
			switch (SelectedPrimitiveType)
			{
			case 0: // StaticMesh
			{
				SpawnStaticMeshActor();
				break;
			}
			case 1: // TextRender
			{
				AActor* Actor = World->SpawnActor<AActor>();
				Actor->SetActorLocation(CurSpawnPoint);
				UTextRenderComponent* Text = Actor->AddComponent<UTextRenderComponent>();
				Actor->SetRootComponent(Text);
				Text->SetFont(FName("Default"));
				Text->SetText("TextRender");
				break;
			}
			case 2: // SubUV
			{
				AActor* Actor = World->SpawnActor<AActor>();
				Actor->SetActorLocation(CurSpawnPoint);
				USubUVComponent* SubUV = Actor->AddComponent<USubUVComponent>();
				Actor->SetRootComponent(SubUV);
				SubUV->SetParticle(FName("Explosion"));
				SubUV->SetSpriteSize(2.0f, 2.0f);
				SubUV->SetFrameRate(30.f);
				break;
			}
			}
		}
		NumberOfSpawnedActors = 1;
	}
	ImGui::InputInt("Number of Spawn", &NumberOfSpawnedActors, 1, 10);

	SEPARATOR();

	// Camera
	FViewportCamera* Camera = EditorEngine->GetCamera();
	if (Camera == nullptr)
	{
		ImGui::End();
		return;
	}

	bool bIsOrtho = (Camera->GetProjectionType() == EViewportProjectionType::Orthographic);
	if (ImGui::Checkbox("Orthographic", &bIsOrtho))
	{
		Camera->SetProjectionType(bIsOrtho ? EViewportProjectionType::Orthographic : EViewportProjectionType::Perspective);
	}

	float CameraFOV_Deg = MathUtil::RadiansToDegrees(Camera->GetFOV());
	if (ImGui::DragFloat("Camera FOV", &CameraFOV_Deg, 0.5f, 1.0f, 90.0f))
	{
		Camera->SetFOV(MathUtil::DegreesToRadians(CameraFOV_Deg));
	}

	float OrthoHeight = Camera->GetOrthoHeight();
	if (ImGui::DragFloat("Ortho Height", &OrthoHeight, 0.1f, 0.1f, 1000.0f))
	{
		Camera->SetOrthoHeight(MathUtil::Clamp(OrthoHeight, 0.1f, 1000.0f));
	}

	FVector CamPos = Camera->GetLocation();
	float CameraLocation[3] = { CamPos.X, CamPos.Y, CamPos.Z };
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f))
	{
		Camera->SetLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
	}

	FVector CamRot = Camera->GetRotation().Rotator().Euler();
	float CameraRotation[3] = { CamRot.X, CamRot.Y, CamRot.Z };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
	{
		FRotator NewRotation = FRotator::MakeFromEuler(FVector(CameraRotation[0], CameraRotation[1], CameraRotation[2]));
		NewRotation.Roll = 0.0f;
		NewRotation.Normalize();
		Camera->SetRotation(NewRotation);
	}

	SEPARATOR();

	// Gizmo Space / Mode
	static int32 SelectedSpace = 0;
	if (ImGui::RadioButton("World", &SelectedSpace, 0))
	{
		EditorEngine->GetGizmo()->SetWorldSpace(true);
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Local", &SelectedSpace, 1))
	{
		EditorEngine->GetGizmo()->SetWorldSpace(false);
	}

	SEPARATOR();

	if (ImGui::Button("Translate")) EditorEngine->GetGizmo()->SetTranslateMode();
	ImGui::SameLine();
	if (ImGui::Button("Rotate")) EditorEngine->GetGizmo()->SetRotateMode();
	ImGui::SameLine();
	if (ImGui::Button("Scale")) EditorEngine->GetGizmo()->SetScaleMode();

	ImGui::End();
}
