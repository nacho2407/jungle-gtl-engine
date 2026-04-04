#include "PreviewViewportClient.h"
#include "Core/ShowFlags.h"

#include "UI/EditorUI.h"
#include "EditorEngine.h"
#include "Renderer/Renderer.h"
#include "Renderer/RenderCommand.h"
#include "Actor/Actor.h"
#include "Component/PrimitiveComponent.h"
#include "Component/CameraComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextComponent.h"
#include "Math/Frustum.h"
#include "World/World.h"
#include "imgui.h"
#include <utility>

FPreviewViewportClient::FPreviewViewportClient(FEditorUI& InEditorUI, FString InPreviewContextName)
	: EditorUI(InEditorUI)
	, PreviewContextName(std::move(InPreviewContextName))
{
}

void FPreviewViewportClient::Attach(FEngine* Engine, FRenderer* Renderer)
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	if (!EditorEngine || !Renderer)
	{
		return;
	}

	EditorUI.Initialize(EditorEngine);
	EditorUI.AttachToRenderer(Renderer);
}

void FPreviewViewportClient::Detach(FEngine* Engine, FRenderer* Renderer)
{
	EditorUI.DetachFromRenderer(Renderer);
}

void FPreviewViewportClient::Tick(FEngine* Engine, float DeltaTime)
{
	if (!Engine)
	{
		return;
	}

	FSlateApplication* Slate = EditorUI.GetEngine()->GetSlateApplication();
	if (!Slate || Slate->GetFocusedViewportId() == INVALID_VIEWPORT_ID)
	{
		return;
	}

	if (ImGui::GetCurrentContext())
	{
		const ImGuiIO& IO = ImGui::GetIO();
		if (IO.WantCaptureKeyboard || IO.WantCaptureMouse)
		{
			return;
		}
	}

	IViewportClient::Tick(Engine, DeltaTime);
}

void FPreviewViewportClient::Render(FEngine* Engine, FRenderer* Renderer)
{
	if (!Engine || !Renderer)
	{
		return;
	}

	UScene* Scene = ResolveScene(Engine);
	UWorld* ActiveWorld = ResolveWorld(Engine);

	if (Scene && ActiveWorld)
	{
		UCameraComponent* ActiveCamera = ActiveWorld->GetActiveCameraComponent();
		if (ActiveCamera)
		{
			FRenderCommandQueue Queue;
			Queue.Reserve(Renderer->GetPrevCommandCount());
			Queue.ViewMatrix = ActiveCamera->GetViewMatrix();
			Queue.ProjectionMatrix = ActiveCamera->GetProjectionMatrix();

			FFrustum Frustum;
			Frustum.ExtractFromVP(Queue.ViewMatrix * Queue.ProjectionMatrix);

			const FMatrix ViewInverse = Queue.ViewMatrix.GetInverse();
			const FVector CameraPosition = ViewInverse.GetTranslation();
			BuildRenderCommands(Engine, Scene, Frustum, FShowFlags{}, CameraPosition, Queue);

			if (FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine))
			{
				AActor* SelectedActor = EditorEngine->GetSelectedActor();
				if (SelectedActor && !SelectedActor->IsPendingDestroy() && SelectedActor->IsVisible())
				{
					for (UActorComponent* Component : SelectedActor->GetComponents())
					{
						if (!Component || !Component->IsA(UPrimitiveComponent::StaticClass()))
						{
							continue;
						}
						if (Component->IsA(UTextComponent::StaticClass()) || Component->IsA(USubUVComponent::StaticClass()))
						{
							continue;
						}

						UPrimitiveComponent* PrimitiveComponent = static_cast<UPrimitiveComponent*>(Component);
						FRenderMesh* RenderMesh = PrimitiveComponent->GetRenderMesh();
						if (!RenderMesh)
						{
							continue;
						}

						FOutlineRenderItem Item = {};
						Item.Mesh = RenderMesh;
						Item.WorldMatrix = PrimitiveComponent->GetWorldTransform();
						Queue.OutlineItems.push_back(Item);
					}
				}
			}

			Renderer->SubmitCommands(std::move(Queue));
			Renderer->ExecuteCommands();
		}
	}

	EditorUI.Render();
}

UScene* FPreviewViewportClient::ResolveScene(FEngine* Engine) const
{
	FEditorEngine* EditorEngine = static_cast<FEditorEngine*>(Engine);
	if (!EditorEngine)
	{
		return nullptr;
	}

	if (UScene* PreviewScene = EditorEngine->GetPreviewScene(PreviewContextName))
	{
		return PreviewScene;
	}

	return EditorEngine->GetActiveScene();
}

