#include "Game/GameRenderPipeline.h"

#include "Game/GameEngine.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerCameraManager.h"
#include "GameFramework/World.h"
#include "Component/CameraComponent.h"
#include "Component/CineCameraComponent.h"
#include "Render/Types/MinimalViewInfo.h"
#include "Input/InputSystem.h"
#include "Viewport/Viewport.h"
#include "Math/MathUtils.h"

namespace
{
	void ApplyLetterboxAspect(FMinimalViewInfo& POV, const FCameraLetterboxState& Letterbox, float ViewportWidth, float ViewportHeight)
	{
		if (!Letterbox.bEnabled || Letterbox.Amount <= 0.0f || ViewportWidth <= 0.0f || ViewportHeight <= 0.0f)
		{
			return;
		}

		const float Thickness = FMath::Clamp(Letterbox.Thickness * Letterbox.Amount, 0.0f, 0.49f);
		const float VisibleHeightScale = 1.0f - Thickness * 2.0f;
		if (VisibleHeightScale <= FMath::Epsilon)
		{
			return;
		}

		POV.AspectRatio = (ViewportWidth / ViewportHeight) / VisibleHeightScale;
	}
}

FGameRenderPipeline::FGameRenderPipeline(UGameEngine* InGame, FRenderer& InRenderer)
	: Game(InGame)
{
}

FGameRenderPipeline::~FGameRenderPipeline()
{
}

void FGameRenderPipeline::Execute(float DeltaTime, FRenderer& Renderer)
{
	ID3D11DeviceContext* Ctx = Renderer.GetFD3DDevice().GetDeviceContext();
	if (!Ctx) return;

	Frame.ClearViewportResources();

	FDrawCommandBuilder& Builder = Renderer.GetBuilder();

	UWorld* World = Game->GetWorld();
	FViewport* VP = Game->GetStandaloneViewport();
	FMinimalViewInfo POV;
	if (!World || !VP || !World->GetActivePOV(POV))
	{
		Renderer.BeginFrame();
		Renderer.EndFrame();
		return;
	}

	Frame.WorldType = World->GetWorldType();

	FViewportRenderOptions Opts;
	Opts.ViewMode = EViewMode::Lit_Phong;
	Frame.SetRenderOptions(Opts);

	FScene* Scene = &World->GetScene();

	PrepareViewport(VP, Ctx);
	BuildFrame(VP, POV, Scene, World);

	FCollectOutput Output;
	CollectCommands(Scene, Renderer, Output);

	Renderer.Render(Frame, *Scene);

	Renderer.BeginFrame();
	Renderer.BlitToBackBuffer(VP->GetSRV());
	Renderer.EndFrame();
}

void FGameRenderPipeline::PrepareViewport(FViewport* VP, ID3D11DeviceContext* Ctx)
{
	if (VP->ApplyPendingResize())
	{
		// OnResize 는 액터 컴포넌트(Pawn 카메라) 본질이라 PC->PlayerCameraManager 경유.
		UWorld* World = Game->GetWorld();
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APlayerCameraManager* CM = PC ? PC->GetPlayerCameraManager() : nullptr;
		if (UCameraComponent* AC = CM ? CM->GetActiveCamera() : nullptr)
		{
			AC->OnResize(static_cast<int32>(VP->GetWidth()), static_cast<int32>(VP->GetHeight()));
		}
	}
	VP->BeginRender(Ctx);
}

void FGameRenderPipeline::BuildFrame(FViewport* VP, const FMinimalViewInfo& POV, FScene* Scene, UWorld* World)
{
	Frame.ClearViewportResources();
	Frame.SetViewportInfo(VP);

	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CamManager = PC ? PC->GetPlayerCameraManager() : nullptr;

	Frame.CameraFade.bEnabled = CamManager ? CamManager->IsFadeEnabled() : false;
	if (Frame.CameraFade.bEnabled)
	{
		Frame.CameraFade.Color = CamManager->GetFadeColor();
		Frame.CameraFade.Amount = CamManager->GetFadeAmount();
	}

	Frame.CameraVignette.bEnabled = CamManager ? CamManager->IsVignetteEnabled() : false;
	if (Frame.CameraVignette.bEnabled)
	{
		Frame.CameraVignette.Intensity = CamManager->GetVignetteIntensity();
		Frame.CameraVignette.Radius = CamManager->GetVignetteRadius();
		Frame.CameraVignette.Softness = CamManager->GetVignetteSoftness();
	}

	UCameraComponent* ActiveCamera = CamManager ? CamManager->GetActiveCamera() : nullptr;
	if (UCineCameraComponent* CineCamera = Cast<UCineCameraComponent>(ActiveCamera))
	{
		const FCineLetterboxSettings& LetterboxSettings = CineCamera->GetLetterboxSettings();
		Frame.CameraLetterbox.bEnabled = LetterboxSettings.bEnabled;
		if (Frame.CameraLetterbox.bEnabled)
		{
			Frame.CameraLetterbox.Amount = LetterboxSettings.Amount;
			Frame.CameraLetterbox.Thickness = LetterboxSettings.Thickness;
			Frame.CameraLetterbox.Color = LetterboxSettings.Color;
		}
	}
	else
	{
		Frame.CameraLetterbox.bEnabled = false;
	}

	FMinimalViewInfo RenderPOV = POV;
	ApplyLetterboxAspect(RenderPOV, Frame.CameraLetterbox, Frame.ViewportWidth, Frame.ViewportHeight);
	Frame.SetCameraInfo(RenderPOV);

	const POINT MousePos = InputSystem::Get().GetMouseClientPos();
	if (MousePos.x >= 0 && MousePos.y >= 0
		&& MousePos.x < static_cast<LONG>(Frame.ViewportWidth)
		&& MousePos.y < static_cast<LONG>(Frame.ViewportHeight))
	{
		Frame.CursorViewportX = static_cast<uint32>(MousePos.x);
		Frame.CursorViewportY = static_cast<uint32>(MousePos.y);
	}
	else
	{
		Frame.CursorViewportX = UINT32_MAX;
		Frame.CursorViewportY = UINT32_MAX;
	}
}

void FGameRenderPipeline::CollectCommands(FScene* Scene, FRenderer& Renderer, FCollectOutput& Output)
{
	FDrawCommandBuilder& Builder = Renderer.GetBuilder();
	Builder.BeginCollect(Frame, Scene->GetProxyCount());

	Collector.Collect(Game->GetWorld(), Frame, Output);
	Builder.BuildCommands(Frame, Scene, Output);
}
