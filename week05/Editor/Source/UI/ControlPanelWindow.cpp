#include "ControlPanelWindow.h"
#include "World/WorldContext.h"
#include "imgui.h"
#include "EditorEngine.h"
#include "Renderer/Renderer.h"
#include "Level/Level.h"
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

#include "Actor/CubeActor.h"
#include "Actor/PlaneActor.h"
#include "Actor/SkySphereActor.h"
#include "Actor/SphereActor.h"
#include "Actor/StaticMeshActor.h"
#include "Actor/SubUVActor.h"
#include "Actor/TextActor.h"
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

		FSlateApplication* Slate = Engine->GetSlateApplication();
		FViewportId FocusedId = Slate ? Slate->GetFocusedViewportId() : INVALID_VIEWPORT_ID;
		FEditorViewportRegistry& ViewportRegistry = Engine->GetViewportRegistry();
		FViewportEntry* Entry = ViewportRegistry.FindEntryByViewportID(FocusedId);
		if (!Entry && !ViewportRegistry.GetEntries().empty())
			Entry = &ViewportRegistry.GetEntries().front();

		// Speed/Sensitivity는 FCamera에서 읽기 (렌더 무관 설정값)
		if (FCamera* Camera = Engine->GetScene()->GetCamera())
		{
			float Sensitivity = Camera->GetMouseSensitivity();
			if (ImGui::SliderFloat("Mouse Sensitivity", &Sensitivity, 0.01f, 1.0f))
				Camera->SetMouseSensitivity(Sensitivity);

			float Speed = Camera->GetSpeed();
			if (ImGui::SliderFloat("Move Speed", &Speed, 0.1f, 20.0f))
				Camera->SetSpeed(Speed);
		}
		if (Entry)
		{
			const bool bIsOrtho = (Entry->LocalState.ProjectionType != EViewportType::Perspective);
			FVector& PositionRef = bIsOrtho ? Entry->LocalState.OrthoTarget : Entry->LocalState.Position;
			float Position[3] = { PositionRef.X, PositionRef.Y, PositionRef.Z };
			if (ImGui::DragFloat3("Position", Position, 0.1f))
				PositionRef = { Position[0], Position[1], Position[2] };

			if (Entry->LocalState.ProjectionType == EViewportType::Perspective)
			{
				float Yaw = Entry->LocalState.Rotation.Yaw;
				float Pitch = Entry->LocalState.Rotation.Pitch;
				bool bRotationChanged = false;
				bRotationChanged |= ImGui::DragFloat("Yaw", &Yaw, 0.5f);
				bRotationChanged |= ImGui::DragFloat("Pitch", &Pitch, 0.5f, -89.0f, 89.0f);
				if (bRotationChanged)
				{
					Entry->LocalState.Rotation.Yaw = Yaw;
					Entry->LocalState.Rotation.Pitch = Pitch;
				}

				float FovY = Entry->LocalState.FovY;
				if (ImGui::SliderFloat("FOV", &FovY, 10.0f, 120.0f))
					Entry->LocalState.FovY = FovY;
			}
			else
			{
				float OrthoZoom = Entry->LocalState.OrthoZoom;
				if (ImGui::DragFloat("Ortho Zoom", &OrthoZoom, 1.0f, 1.0f, 10000.0f))
					Entry->LocalState.OrthoZoom = OrthoZoom;
			}
		}

		ImGui::SeparatorText("Spawn");

		static int32 SpawnTypeIndex = 0;
		const char* SpawnTypes[] = { "Cube", "Sphere", "Plane", "SubUV", "Text", "SkySphere", "Staticmesh" };

		ImGui::Combo("Type", &SpawnTypeIndex, SpawnTypes, IM_ARRAYSIZE(SpawnTypes));

		static char SpawnTextBuffer[256] = "Text";


		if (SpawnTypeIndex == 4)
		{
			ImGui::InputText("Text", SpawnTextBuffer, IM_ARRAYSIZE(SpawnTextBuffer));
		}

		if (ImGui::Button("Spawn"))
		{
			ULevel* Scene = Engine->GetScene();
			static int32 SpawnCount = 0;
			const FString Name = FString(SpawnTypes[SpawnTypeIndex]) + "_Spawned_" + std::to_string(SpawnCount++);

			AActor* NewActor = nullptr;

			// ─── 1. 특수 액터 (미리 조립된 테스트용 액터) ───
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
			// ─── 2. 순수 컴포넌트 조립 방식 (대통합!) ───
			else if (SpawnTypeIndex == 3)
			{
				NewActor = Scene->SpawnActor<ASubUVActor>(Name);
			}
			else if (SpawnTypeIndex == 4)
			{
				NewActor = Scene->SpawnActor<ATextActor>(Name);

				if (NewActor)
				{
					ATextActor* TextActor = static_cast<ATextActor*>(NewActor);
					if (UTextRenderComponent* TextComponent = TextActor->GetComponentByClass<UTextRenderComponent>())
					{
						if (SpawnTextBuffer[0] != '\0') TextComponent->SetText(SpawnTextBuffer);
						else TextComponent->SetText("Text");
					}
				}
			}
			else if (SpawnTypeIndex == 5)
			{
				NewActor = Scene->SpawnActor<ASkySphereActor>(Name);
			}
			else
			{
				NewActor = Scene->SpawnActor<AActor>(Name);
				if (NewActor)
				{
					UStaticMeshComponent* MeshComp = FObjectFactory::ConstructObject<UStaticMeshComponent>(nullptr, "StaticMeshComponent");

					std::filesystem::path ModelPath = FPaths::MeshDir() / "cube-tex.obj";
					FString FullPath = FPaths::FromPath(ModelPath);

					UStaticMesh* MeshData = FObjManager::LoadObjStaticMeshAsset(FullPath);
					if (MeshData)
					{
						MeshComp->SetStaticMesh(MeshData);
						UE_LOG("[테스트] OBJ 파일 로드 성공! 섹션 개수: %d", MeshData->GetNumSections());

						MeshComp->SetRelativeLocation(FVector(0, 0, 3.0f));
					}
					else
					{
						UE_LOG("[테스트 실패] OBJ 파일을 찾을 수 없거나 파싱에 실패했습니다.");
					}
					NewActor->AddOwnedComponent(MeshComp);
					NewActor->SetRootComponent(MeshComp);
				}
			}

			// ─── 마무리: 에디터 선택 및 로그 출력 ───
			// 하늘(6번)은 배경이므로 에디터에서 자동 선택되지 않게 막아둠
			if (NewActor && SpawnTypeIndex != 5)
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
