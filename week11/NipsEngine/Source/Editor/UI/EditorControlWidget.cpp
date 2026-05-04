#include "Editor/UI/EditorControlWidget.h"

#include "Editor/EditorEngine.h"
#include "Camera/ViewportCamera.h"
#include "Core/Logging/Timer.h"

#include "ImGui/imgui.h"
#include "Component/GizmoComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Core/ResourceManager.h"
#include "Runtime/Script/ScriptManager.h"
#include "Runtime/Script/ScriptComponent.h"

#include "GameFramework/PrimitiveActors.h"

#define SEPARATOR(); ImGui::Spacing(); ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing(); ImGui::Spacing();

namespace
{
	bool HasPlayerStart(UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		for (AActor* Actor : World->GetActors())
		{
			if (Actor && Actor->IsA<APlayerStart>())
			{
				return true;
			}
		}
		return false;
	}

	bool HasPlacedPlayer(UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		for (AActor* Actor : World->GetActors())
		{
			if (Actor && Actor->HasTag("Player"))
			{
				return true;
			}
		}
		return false;
	}
}

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

bool FEditorControlWidget::DrawPlaceActorMenu(const FVector& SpawnPoint, bool bClosePopupOnSpawn)
{
	bool bSpawned = false;
	auto DrawSpawnItem = [&](int32 PrimitiveType, const char* Label)
	{
		if (ImGui::MenuItem(Label))
		{
			bSpawned = SpawnPrimitive(PrimitiveType, SpawnPoint, 1) || bSpawned;
			if (bSpawned && bClosePopupOnSpawn)
			{
				ImGui::CloseCurrentPopup();
			}
		}
	};

	DrawSpawnItem(0, "Empty Actor");
	ImGui::Separator();
	DrawSpawnItem(18, "Player");
	DrawSpawnItem(1, "Static Mesh");
	DrawSpawnItem(2, "Text Render");
	DrawSpawnItem(3, "SubUV");
	DrawSpawnItem(4, "Billboard");
	DrawSpawnItem(5, "Decal");
	DrawSpawnItem(12, "Fog");
	DrawSpawnItem(6, "Fireball");
    DrawSpawnItem(7, "Decal Spotlight");
    DrawSpawnItem(14, "Cube");
    DrawSpawnItem(15, "Destructible");
    DrawSpawnItem(16, "BondingBox");
	DrawSpawnItem(17, "Main Scene Destructible");
	ImGui::Separator();
	DrawSpawnItem(8, "Ambient Light");
	DrawSpawnItem(9, "Directional Light");
	DrawSpawnItem(10, "Point Light");
	DrawSpawnItem(11, "Spot Light");
	ImGui::Separator();
	DrawSpawnItem(13, "Player Start");
	return bSpawned;
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
	if (PrimitiveType == 13)
	{
		Count = 1;
		if (HasPlayerStart(World))
		{
			EditorEngine->GetMainPanel().PushFooterLog("Player Start already exists");
			return false;
		}
	}
	if (PrimitiveType == 17)
	{
		Count = 1;
		if (HasPlacedPlayer(World))
		{
			EditorEngine->GetMainPanel().PushFooterLog("Player actor already exists");
			return false;
		}
	}

	EditorEngine->CaptureUndoSnapshot("Place Actor");
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
		case 12:
		{
			AFogActor* Actor = World->SpawnActor<AFogActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 13:
		{
			APlayerStart* Actor = World->SpawnActor<APlayerStart>();
			Actor->InitDefaultComponents();
			Actor->SetFName(FName("Player Start"));
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 14:
		{
			ACubeActor* Actor = World->SpawnActor<ACubeActor>();
			Actor->InitDefaultComponents();
			Actor->SetActorLocation(SpawnPoint);
			break;
		}
		case 15:
        {
            ADestructibleActor* Actor = World->SpawnActor<ADestructibleActor>();
            Actor->InitDefaultComponents();
            Actor->SetActorLocation(SpawnPoint);
            break;
		}
		case 16:
        {
            ABoundsBoxActor* Actor = World->SpawnActor<ABoundsBoxActor>();
            Actor->InitDefaultComponents();
            Actor->SetActorLocation(SpawnPoint);
            break;
		}
		case 17:
		{
		    AMainSceneDestructibleActor* Actor = World->SpawnActor<AMainSceneDestructibleActor>();
		    Actor->InitDefaultComponents();
		    Actor->SetActorLocation(SpawnPoint);
		    break;
		}
		case 18:
		{
			ADefaultPlayerActor* Actor = World->SpawnActor<ADefaultPlayerActor>();
			Actor->InitDefaultComponents();
			Actor->SetFName(FName("Player"));
			Actor->AddTag("Player");
			Actor->SetActorLocation(SpawnPoint);
			Actor->AddComponent<UScriptComponent>();
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
	ImGui::SetNextWindowSize(ImVec2(360.0f, 180.0f), ImGuiCond_Once);

	ImGui::Begin("Jungle Control Panel");
	
	ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x * 0.5f);

	// Camera
	FViewportCamera* Camera = EditorEngine->GetCamera();
	if (Camera == nullptr)
	{
		ImGui::End();
		return;
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

	ImGui::End();
}
