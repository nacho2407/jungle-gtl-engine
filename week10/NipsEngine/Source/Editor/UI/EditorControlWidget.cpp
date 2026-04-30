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

#include "GameFramework/PrimitiveActors.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

void FEditorControlWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
	SelectedPrimitiveType = 0;
}

const char* FEditorControlWidget::GetPrimitiveTypeLabel(int32 PrimitiveType) const
{
	if (PrimitiveType < 0 || PrimitiveType >= GetPrimitiveTypeCount())
	{
		return "";
	}
	return PrimitiveTypes[PrimitiveType];
}

bool FEditorControlWidget::SpawnPrimitive(int32 PrimitiveType, const FVector& SpawnPoint, int32 Count)
{
	if (!EditorEngine)
	{
		return false;
	}

	UWorld* World = EditorEngine->GetFocusedWorld();
	if (!World)
	{
		return false;
	}

	Count = MathUtil::Clamp(Count, 1, 100);
	for (int32 i = 0; i < Count; i++)
	{
		switch (PrimitiveType)
		{
		case 0:
		{
			ASceneActor* Actor = World->SpawnActor<ASceneActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 1:
		{
			AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 2:
		{
			ATextRenderActor* Actor = World->SpawnActor<ATextRenderActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 3:
		{
			ASubUVActor* Actor = World->SpawnActor<ASubUVActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 4:
		{
			ABillboardActor* Actor = World->SpawnActor<ABillboardActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 5:
		{
			ADecalActor* Actor = World->SpawnActor<ADecalActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 6:
		{
			AFireballActor* Actor = World->SpawnActor<AFireballActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 7:
		{
			ADecalSpotLightActor* Actor = World->SpawnActor<ADecalSpotLightActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 8:
		{
			AAmbientLightActor* Actor = World->SpawnActor<AAmbientLightActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 9:
		{
			ADirectionalLightActor* Actor = World->SpawnActor<ADirectionalLightActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 10:
		{
			APointLightActor* Actor = World->SpawnActor<APointLightActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 11:
		{
			ASpotlightActor* Actor = World->SpawnActor<ASpotlightActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		default:
			return false;
		}
	}

	return true;
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
	
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

	// Spawn
	ImGui::Combo("Actor", &SelectedPrimitiveType, PrimitiveTypes, IM_ARRAYSIZE(PrimitiveTypes));

	if (ImGui::Button("Spawn"))
	{
		if (SpawnPrimitive(SelectedPrimitiveType, CurSpawnPoint, NumberOfSpawnedActors))
		{
			NumberOfSpawnedActors = 1;
		}
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
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f, 0.0f, 0.0f, "%.1f"))
	{
		Camera->SetLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
	}

	FVector CamRot = Camera->GetRotation().Rotator().Euler();
	float CameraRotation[3] = { CamRot.X, CamRot.Y, CamRot.Z };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f, 0.0f, 0.0f, "%.1f"))
	{
		CameraRotation[1] = MathUtil::Clamp(CameraRotation[1], -89.9f, 89.9f);
        FRotator NewRotation = FRotator::MakeFromEuler(FVector(CameraRotation[0], CameraRotation[1], CameraRotation[2]));

		NewRotation.Normalize();
		Camera->SetRotation(NewRotation);
	}

	ImGui::PopItemWidth();

	SEPARATOR();

	// Gizmo Space / Mode
	int32 SelectedSpace = EditorEngine->GetGizmo()->IsWorldSpace() ? 0 : 1;
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
