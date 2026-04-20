#include "Editor/UI/EditorControlWidget.h"
#include "Editor/EditorEngine.h"
#include "Engine/Profiling/Timer.h"
#include "Engine/Profiling/MemoryStats.h"
#include "ImGui/imgui.h"
#include "Component/CameraComponent.h"
#include "Component/GizmoComponent.h"
#include "GameFramework/DecalActor.h"
#include "GameFramework/FakeLightActor.h"
#include "GameFramework/FireballActor.h"
#include "GameFramework/HeightFogActor.h"
#include "GameFramework/StaticMeshActor.h"
#include "GameFramework/AmbientLightActor.h"
#include "GameFramework/DirectionalLightActor.h"
#include "GameFramework/PointLightActor.h"
#include "GameFramework/SpotLightActor.h"

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

	if (ImGui::CollapsingHeader("Grid Spawn"))
	{
		ImGui::DragFloat3("Center Offset", GridCenterOffset, 0.5f);
		ImGui::InputInt("Count X", &GridCountX, 1, 5);
		ImGui::InputInt("Count Y", &GridCountY, 1, 5);
		ImGui::DragFloat("Spacing X", &GridSpacingX, 0.1f, 0.1f, 100.f);
		ImGui::DragFloat("Spacing Y", &GridSpacingY, 0.1f, 0.1f, 100.f);
		if (GridCountX < 1) GridCountX = 1;
		if (GridCountY < 1) GridCountY = 1;

		if (ImGui::Button("Spawn Grid"))
		{
			UWorld* World = EditorEngine->GetWorld();
			FVector Center(GridCenterOffset[0], GridCenterOffset[1], GridCenterOffset[2]);
			float HalfX = (GridCountX - 1) * GridSpacingX * 0.5f;
			float HalfY = (GridCountY - 1) * GridSpacingY * 0.5f;

			for (int32 iy = 0; iy < GridCountY; ++iy)
			{
				for (int32 ix = 0; ix < GridCountX; ++ix)
				{
					FVector Pos = Center + FVector(ix * GridSpacingX - HalfX, iy * GridSpacingY - HalfY, 0.f);

					switch (SelectedPrimitiveType)
					{
					case 0:
					{
						AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
						Actor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
						Actor->SetActorLocation(Pos);
						World->InsertActorToOctree(Actor);
						break;
					}
					case 1:
					{
						AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
						Actor->InitDefaultComponents("Data/BasicShape/Sphere.OBJ");
						Actor->SetActorLocation(Pos);
						World->InsertActorToOctree(Actor);
						break;
					}
					case 2:
					{
						ADecalActor* Actor = World->SpawnActor<ADecalActor>();
						Actor->InitDefaultComponents();
						Actor->SetActorLocation(Pos);
						World->InsertActorToOctree(Actor);
						break;
					}
					case 3:
					{
						AHeightFogActor* Actor = World->SpawnActor<AHeightFogActor>();
						Actor->InitDefaultComponents();
						Actor->SetActorLocation(Pos);
						break;
					}
					case 4:
					{
						AFakeLightActor* Actor = World->SpawnActor<AFakeLightActor>();
						Actor->InitDefaultComponents();
						Actor->SetActorLocation(Pos);
						break;
					}
					case 5:
					{
						AFireballActor* Actor = World->SpawnActor<AFireballActor>();
						Actor->InitDefaultComponents();
						Actor->SetActorLocation(Pos);
						break;
					}
					case 6:
					{
						AAmbientLightActor* Actor = World->SpawnActor<AAmbientLightActor>();
						Actor->InitDefaultComponents();
						Actor->SetActorLocation(Pos);
						break;
					}
					case 7:
					{
						ADirectionalLightActor* Actor = World->SpawnActor<ADirectionalLightActor>();
						Actor->InitDefaultComponents();
						Actor->SetActorLocation(Pos);
						break;
					}
					case 8:
					{
						APointLightActor* Actor = World->SpawnActor<APointLightActor>();
						Actor->InitDefaultComponents();
						Actor->SetActorLocation(Pos);
						break;
					}
					case 9:
					{
						ASpotLightActor* Actor = World->SpawnActor<ASpotLightActor>();
						Actor->InitDefaultComponents();
						Actor->SetActorLocation(Pos);
						break;
					}
					}
				}
			}
		}
	}

	if (ImGui::Button("Spawn"))
	{
		UWorld* World = EditorEngine->GetWorld();
		for (int32 i = 0; i < NumberOfSpawnedActors; i++)
		{
			switch (SelectedPrimitiveType)
			{
			case 0:
			{
				AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
				Actor->SetActorLocation(CurSpawnPoint);
				Actor->InitDefaultComponents("Data/BasicShape/Cube.OBJ");
				World->InsertActorToOctree(Actor);
				break;
			}
			case 1:
			{
				AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>();
				Actor->SetActorLocation(CurSpawnPoint);
				Actor->InitDefaultComponents("Data/BasicShape/Sphere.OBJ");
				World->InsertActorToOctree(Actor);
				break;
			}
			case 2:
			{
				ADecalActor* Actor = World->SpawnActor<ADecalActor>();
				Actor->InitDefaultComponents();
				Actor->SetActorLocation(CurSpawnPoint);
				World->InsertActorToOctree(Actor);
				break;
			}
			case 3:
			{
				AHeightFogActor* Actor = World->SpawnActor<AHeightFogActor>();
				Actor->InitDefaultComponents();
				Actor->SetActorLocation(CurSpawnPoint);
				break;
			}
			case 4:
			{
				AFakeLightActor* Actor = World->SpawnActor<AFakeLightActor>();
				Actor->InitDefaultComponents();
				Actor->SetActorLocation(CurSpawnPoint);
				break;
			}
			case 5:
			{
				AFireballActor* Actor = World->SpawnActor<AFireballActor>();
				Actor->InitDefaultComponents();
				Actor->SetActorLocation(CurSpawnPoint);
				break;
			}
			case 6:
			{
				AAmbientLightActor* Actor = World->SpawnActor<AAmbientLightActor>();
				Actor->InitDefaultComponents();
				Actor->SetActorLocation(CurSpawnPoint);
				break;
			}
			case 7:
			{
				ADirectionalLightActor* Actor = World->SpawnActor<ADirectionalLightActor>();
				Actor->InitDefaultComponents();
				Actor->SetActorLocation(CurSpawnPoint);
				break;
			}
			case 8:
			{
				APointLightActor* Actor = World->SpawnActor<APointLightActor>();
				Actor->InitDefaultComponents();
				Actor->SetActorLocation(CurSpawnPoint);
				break;
			}
			case 9:
			{
				ASpotLightActor* Actor = World->SpawnActor<ASpotLightActor>();
				Actor->InitDefaultComponents();
				Actor->SetActorLocation(CurSpawnPoint);
				break;
			}
			}
		}
		NumberOfSpawnedActors = 1;
	}
	ImGui::InputInt("Number of Spawn", &NumberOfSpawnedActors, 1, 10);

	SEPARATOR();

	// Camera
	UCameraComponent* Camera = EditorEngine->GetCamera();

	float CameraFOV_Deg = Camera->GetFOV() * RAD_TO_DEG;
	if (ImGui::DragFloat("Camera FOV", &CameraFOV_Deg, 0.5f, 1.0f, 90.0f))
	{
		Camera->SetFOV(CameraFOV_Deg * DEG_TO_RAD);
	}

	float OrthoWidth = Camera->GetOrthoWidth();
	if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.1f, 1000.0f))
	{
		Camera->SetOrthoWidth(Clamp(OrthoWidth, 0.1f, 1000.0f));
	}

	FVector CamPos = Camera->GetWorldLocation();
	float CameraLocation[3] = { CamPos.X, CamPos.Y, CamPos.Z };
	if (ImGui::DragFloat3("Camera Location", CameraLocation, 0.1f))
	{
		Camera->SetWorldLocation(FVector(CameraLocation[0], CameraLocation[1], CameraLocation[2]));
	}

	FRotator CamRot = Camera->GetRelativeRotation();
	float CameraRotation[3] = { CamRot.Roll, CamRot.Pitch, CamRot.Yaw };
	if (ImGui::DragFloat3("Camera Rotation", CameraRotation, 0.1f))
	{
		Camera->SetRelativeRotation(FRotator(CameraRotation[1], CameraRotation[2], CamRot.Roll));
	}

	ImGui::End();
}
