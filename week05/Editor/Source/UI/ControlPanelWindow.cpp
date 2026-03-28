#include "ControlPanelWindow.h"
#include "World/WorldContext.h"
#include "imgui.h"
#include "Core/Core.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "Actor/Actor.h"
#include "Actor/AttachTestActor.h"
#include "Actor/CubeActor.h"
#include "Actor/SphereActor.h"
#include "Actor/PlaneActor.h"
#include "Actor/SubUVActor.h"
#include "Actor/TextActor.h"
#include "Component/TextComponent.h"
#include "Object/ObjectFactory.h"
#include "Camera/Camera.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Component/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Actor/SkySphereActor.h"
#include "Controller/EditorViewportController.h"
#include "Serializer/SceneSerializer.h"
#include <filesystem>
#include <random>
#include <chrono>

#include "Actor/StaticMeshActor.h"
#include "Obj/ObjManager.h"

namespace
{
	const char* GetSceneTypeLabel(ESceneType SceneType)
	{
		switch (SceneType)
		{
		case ESceneType::Game:
			return "Game";
		case ESceneType::Editor:
			return "Editor";
		case ESceneType::PIE:
			return "PIE";
		case ESceneType::Preview:
			return "Preview";
		case ESceneType::Inactive:
			return "Inactive";
		default:
			return "Unknown";
		}
	}
}

void CControlPanelWindow::Render(CCore* Core)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	const bool bOpen = ImGui::Begin("Control Panel");
	ImGui::PopStyleVar();

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	if (Core && Core->GetScene())
	{
	
		const FWorldContext* ActiveSceneContext = Core->GetActiveWorldContext();
		const TArray<std::unique_ptr<FEditorWorldContext>>& PreviewSceneContexts = Core->GetSceneManager()->GetPreviewWorldContexts();
		const bool bPreviewActive = ActiveSceneContext && ActiveSceneContext->WorldType == ESceneType::Preview;

		/*
			PreviewScene 등 아마 확장의 여지를 둔 것으로 보이나 아무 기능도 없어 주석 처리함		
		*/
		/*
		ImGui::SeparatorText("World");

		if (ActiveSceneContext)
		{
			ImGui::Text("Active: %s", ActiveSceneContext->ContextName.c_str());
			ImGui::Text("Type: %s", GetSceneTypeLabel(ActiveSceneContext->WorldType));
		}
		*/

		/*
		if (ImGui::Button("Editor Scene"))
		{
			Core->ActivateEditorScene();
		}
		*/

		/*
		ImGui::SameLine();

		if (PreviewSceneContexts.empty())
		{
			ImGui::BeginDisabled();
			ImGui::Button("Preview Scene");
			ImGui::EndDisabled();
		}
		else if (ImGui::Button("Preview Scene"))
		{
			Core->ActivatePreviewScene(PreviewSceneContexts.front()->ContextName);
		}

		if (bPreviewActive)
		{
			ImGui::TextUnformatted("Preview scene is editor-only. Scene save/load is disabled.");
		}
		*/

		ImGui::SeparatorText("Camera");
		

		/*
		if (ImGui::Button("Spawn Test"))
		{
			UScene* Scene = Core->GetScene();
			AActor* NewActor = nullptr;

			for (int i = 0; i < 1000; i++)
			{
				// 시드: 현재 시간 기반
				static std::mt19937 rng(static_cast<unsigned int>(
					std::chrono::steady_clock::now().time_since_epoch().count()
					));

				std::uniform_real_distribution<float> dist(-10, 10);

				FVector V{ 0, 0, 0 };
				NewActor = Scene->SpawnActor<ACubeActor>("Test");
				NewActor->SetActorLocation(V);
			}
		}
		*/
		
		if (CCamera* Camera = Core->GetScene()->GetCamera())
		{
		
			float Sensitivity = Camera->GetMouseSensitivity();
			if (ImGui::SliderFloat("Mouse Sensitivity", &Sensitivity, 0.01f, 1.0f))
			{
				Camera->SetMouseSensitivity(Sensitivity);
			}

			float Speed = Camera->GetSpeed();
			if (ImGui::SliderFloat("Move Speed", &Speed, 0.1f, 20.0f))
			{
				Camera->SetSpeed(Speed);
			}
			const FVector CameraPosition = Camera->GetPosition();
			float Position[3] = { CameraPosition.X, CameraPosition.Y, CameraPosition.Z };
			if (ImGui::DragFloat3("Position", Position, 0.1f))
			{
				Camera->SetPosition({ Position[0], Position[1], Position[2] });
			}

			float CameraYaw = Camera->GetYaw();
			float CameraPitch = Camera->GetPitch();
			bool bRotationChanged = false;
			bRotationChanged |= ImGui::DragFloat("Yaw", &CameraYaw, 0.5f);
			bRotationChanged |= ImGui::DragFloat("Pitch", &CameraPitch, 0.5f, -89.0f, 89.0f);
			if (bRotationChanged)
			{
				Camera->SetRotation(CameraYaw, CameraPitch);
			}

			int ProjectionModeIndex = (Camera->GetProjectionMode() == ECameraProjectionMode::Orthographic) ? 1 : 0;
			const char* ProjectionModes[] = { "Perspective", "Orthographic" };
			if (ImGui::Combo("Projection", &ProjectionModeIndex, ProjectionModes, IM_ARRAYSIZE(ProjectionModes)))
			{
				Camera->SetProjectionMode(
					ProjectionModeIndex == 0
					? ECameraProjectionMode::Perspective
					: ECameraProjectionMode::Orthographic);
			}

			if (Camera->IsOrthographic())
			{
				float OrthoWidth = Camera->GetOrthoWidth();
				if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.5f, 1.0f, 1000.0f))
				{
					Camera->SetOrthoWidth(OrthoWidth);
				}
			}
			else
			{
				float CameraFOV = Camera->GetFOV();
				if (ImGui::SliderFloat("FOV", &CameraFOV, 10.0f, 120.0f))
				{
					Camera->SetFOV(CameraFOV);
				}
			}
		}

		ImGui::SeparatorText("Spawn");

		static int32 SpawnTypeIndex = 0;
		const char* SpawnTypes[] = { "Cube", "Sphere", "Plane", "AttachTest", "SubUV", "Text", "SkySphere", "Staticmesh" };

		ImGui::Combo("Type", &SpawnTypeIndex, SpawnTypes, IM_ARRAYSIZE(SpawnTypes));

		static char SpawnTextBuffer[256] = "Text";


		if (SpawnTypeIndex == 5)
		{
			ImGui::InputText("Text", SpawnTextBuffer, IM_ARRAYSIZE(SpawnTextBuffer));
		}

		if (ImGui::Button("Spawn"))
		{
			UScene* Scene = Core->GetScene();
			static int32 SpawnCount = 0;
			const FString Name = FString(SpawnTypes[SpawnTypeIndex]) + "_Spawned_" + std::to_string(SpawnCount++);

			AActor* NewActor = nullptr;
			if (SpawnTypeIndex == 0)
			{
				NewActor = Scene->SpawnActor<ACubeActor>(Name);
			}
			else if (SpawnTypeIndex == 1)
			{
				NewActor = Scene->SpawnActor<ASphereActor>(Name);
			}
			else if (SpawnTypeIndex == 2)
			{
				NewActor = Scene->SpawnActor<APlaneActor>(Name);
			}
			else if (SpawnTypeIndex == 3)
			{
				NewActor = Scene->SpawnActor<AAttachTestActor>(Name);
			}
			else if (SpawnTypeIndex == 4)
			{
				NewActor = Scene->SpawnActor<ASubUVActor>(Name);
			}
			else if (SpawnTypeIndex == 5)
			{
				NewActor = Scene->SpawnActor<ATextActor>(Name);

				if (NewActor)
				{
					ATextActor* TextActor = static_cast<ATextActor*>(NewActor);
					if (UTextComponent* TextComponent = TextActor->GetTextComponent())
					{
						if (SpawnTextBuffer[0] != '\0')
						{
							TextComponent->SetText(SpawnTextBuffer);
						}
						else
						{
							TextComponent->SetText("Text");
						}
					}
				}
			}
			else if (SpawnTypeIndex == 6)
			{
				NewActor = Scene->SpawnActor<ASkySphereActor>(Name);
			}
			else if (SpawnTypeIndex == 7)
			{
				AStaticMeshActor* MyCube = Scene->SpawnActor<AStaticMeshActor>(Name);
				if (MyCube && MyCube->GetStaticMeshComponent())
				{
					UStaticMesh* CubeData = FObjManager::GetPrimitivePlane();
					MyCube->GetStaticMeshComponent()->SetStaticMesh(CubeData);

					// 위치도 적당히 옮겨봅니다.
					MyCube->SetActorLocation(FVector(0.0f, 0.0f, 5.0f));
				}
			}

			if (NewActor && !NewActor->IsA<ASkySphereActor>())
			{
				Core->SetSelectedActor(NewActor);
			}
			UE_LOG("Spawned %s: %s", SpawnTypes[SpawnTypeIndex], Name.c_str());
		}

		ImGui::SameLine();
		AActor* SelectedActor = Core->GetSelectedActor();
		if (!SelectedActor)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Delete"))
		{
			const FString Name = SelectedActor->GetName();
			Core->GetScene()->DestroyActor(SelectedActor);
			Core->SetSelectedActor(nullptr);
			UE_LOG("Deleted actor: %s", Name.c_str());
		}

		if (!SelectedActor)
		{
			ImGui::EndDisabled();
		}
	}

	ImGui::End();
}
