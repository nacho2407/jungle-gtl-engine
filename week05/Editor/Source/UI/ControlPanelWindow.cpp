#include "ControlPanelWindow.h"
#include "World/WorldContext.h"
#include "imgui.h"
#include "EditorEngine.h"
#include "Renderer/Renderer.h"
#include "Scene/Scene.h"
#include "Actor/Actor.h"
#include "Component/TextComponent.h"
#include "Component/SkyComponent.h"
#include "Object/ObjectFactory.h"
#include "Camera/Camera.h"
#include "Core/Paths.h"
#include "Debug/EngineLog.h"
#include "Component/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Controller/EditorViewportController.h"
#include "Serializer/SceneSerializer.h"
#include <filesystem>
#include <random>
#include <chrono>

#include "Actor/StaticMeshActor.h"
#include "Math/MathUtility.h"
#include "Asset/ObjManager.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"

namespace
{
	const char* GetWorldTypeLabel(EWorldType WorldType)
	{
		switch (WorldType)
		{
		case EWorldType::Game:
			return "Game";
		case EWorldType::Editor:
			return "Editor";
		case EWorldType::PIE:
			return "PIE";
		case EWorldType::Preview:
			return "Preview";
		case EWorldType::Inactive:
			return "Inactive";
		default:
			return "Unknown";
		}
	}
}

void FControlPanelWindow::Render(FEditorEngine* Engine)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	const bool bOpen = ImGui::Begin("Control Panel");
	ImGui::PopStyleVar();

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	if (Engine && Engine->GetScene())
	{
	
		const FWorldContext* ActiveSceneContext = Engine->GetActiveWorldContext();
		const TArray<FWorldContext*>& PreviewSceneContexts = Engine->GetPreviewWorldContexts();
		const bool bPreviewActive = ActiveSceneContext && ActiveSceneContext->WorldType == EWorldType::Preview;

		/*
			PreviewScene 등 아마 확장의 여지를 둔 것으로 보이나 아무 기능도 없어 주석 처리함		
		*/
		/*
		ImGui::SeparatorText("World");

		if (ActiveSceneContext)
		{
			ImGui::Text("Active: %s", ActiveSceneContext->ContextName.c_str());
			ImGui::Text("Type: %s", GetWorldTypeLabel(ActiveSceneContext->WorldType));
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
		
		if (FCamera* Camera = Engine->GetScene()->GetCamera())
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
			UScene* Scene = Engine->GetScene();
			static int32 SpawnCount = 0;
			const FString Name = FString(SpawnTypes[SpawnTypeIndex]) + "_Spawned_" + std::to_string(SpawnCount++);

			AActor* NewActor = nullptr;

			// ─── 1. 특수 액터 (미리 조립된 테스트용 액터) ───
			if (SpawnTypeIndex == 3)
			{
				// NewActor = Scene->SpawnActor<AAttachTestActor>(Name);
			}
			// ─── 2. 순수 컴포넌트 조립 방식 (대통합!) ───
			else
			{
				// 도화지가 될 텅 빈 액터 스폰
				AActor* EmptyActor = Scene->SpawnActor<AActor>(Name);

				if (EmptyActor)
				{
					// [0, 1, 2, 7번] 스태틱 메시 계열 (Cube, Sphere, Plane)
					if (SpawnTypeIndex == 0 || SpawnTypeIndex == 1 || SpawnTypeIndex == 2 || SpawnTypeIndex == 7)
					{
						UStaticMeshComponent* MeshComp = FObjectFactory::ConstructObject<UStaticMeshComponent>(EmptyActor);
						EmptyActor->AddOwnedComponent(MeshComp);
						EmptyActor->SetRootComponent(MeshComp);

						UStaticMesh* MeshData = nullptr;
						if (SpawnTypeIndex == 0)      MeshData = FObjManager::LoadObjStaticMeshAsset((FPaths::MeshDir() / "PrimitiveBox.obj").string().c_str());
						else if (SpawnTypeIndex == 1) MeshData = FObjManager::GetPrimitiveSphere();
						else if (SpawnTypeIndex == 2)  MeshData = FObjManager::LoadObjStaticMeshAsset((FPaths::MeshDir() / "PrimitivePlane.obj").string().c_str());
						else if (SpawnTypeIndex == 7) 
						{
							std::filesystem::path ModelPath = FPaths::MeshDir() / "cube-tex.obj";
							FString FullPath = ModelPath.string().c_str(); // FString으로 변환

							MeshData = FObjManager::LoadObjStaticMeshAsset(FullPath);
							if (MeshData)
							{
								MeshComp->SetStaticMesh(MeshData);
								UE_LOG("[테스트] OBJ 파일 로드 성공! 섹션 개수: %d", MeshData->GetNumSections());

								// 눈에 잘 보이게 위치 조정
								MeshComp->SetRelativeLocation(FVector(0, 0, 3.0f));
							}
							else
							{
								UE_LOG("[테스트 실패] OBJ 파일을 찾을 수 없거나 파싱에 실패했습니다.");
							}
						}

						MeshComp->SetStaticMesh(MeshData);
					}
					// [4번] SubUV 스프라이트
					else if (SpawnTypeIndex == 4)
					{
						USubUVComponent* SubUVComp = FObjectFactory::ConstructObject<USubUVComponent>(EmptyActor);
						EmptyActor->AddOwnedComponent(SubUVComp);
						EmptyActor->SetRootComponent(SubUVComp);
					}
					// [5번] 텍스트 (Text)
					else if (SpawnTypeIndex == 5)
					{
						UTextComponent* TextComp = FObjectFactory::ConstructObject<UTextComponent>(EmptyActor);
						EmptyActor->AddOwnedComponent(TextComp);
						EmptyActor->SetRootComponent(TextComp);

						TextComp->SetText(SpawnTextBuffer[0] != '\0' ? SpawnTextBuffer : "Text");
					}
					// [6번] 하늘 (SkySphere)
					else if (SpawnTypeIndex == 6)
					{
						USkyComponent* SkyComp = FObjectFactory::ConstructObject<USkyComponent>(EmptyActor);
						EmptyActor->AddOwnedComponent(SkyComp);
						EmptyActor->SetRootComponent(SkyComp);

						// USkyComponent는 스스로 Initialize()에서 FObjManager::GetPrimitiveSky()를 세팅하므로 
						// 여기서 따로 메쉬를 넣어줄 필요가 없습니다!
					}

					NewActor = EmptyActor;
				}
			}

			// ─── 마무리: 에디터 선택 및 로그 출력 ───
			// 하늘(6번)은 배경이므로 에디터에서 자동 선택되지 않게 막아둠
			if (NewActor && SpawnTypeIndex != 6)
			{
				Engine->SetSelectedActor(NewActor);
			}
			UE_LOG("Spawned %s: %s", SpawnTypes[SpawnTypeIndex], Name.c_str());
		}

		ImGui::SameLine();
		AActor* SelectedActor = Engine->GetSelectedActor();
		if (!SelectedActor)
		{
			ImGui::BeginDisabled();
		}

		if (ImGui::Button("Delete"))
		{
			const FString Name = SelectedActor->GetName();
			Engine->GetScene()->DestroyActor(SelectedActor);
			Engine->SetSelectedActor(nullptr);
			UE_LOG("Deleted actor: %s", Name.c_str());
		}

		if (!SelectedActor)
		{
			ImGui::EndDisabled();
		}
	}

	ImGui::End();
}
