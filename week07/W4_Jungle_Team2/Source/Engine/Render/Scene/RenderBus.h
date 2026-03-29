#pragma once

/*

	는 Renderer에게 Draw Call 요청을 vector의 형태로 전달하는 역할을 합니다.
	Renderer가 RenderBus에 담긴 Draw Call 요청들을 처리할 수 있게 합니다.
*/

//	TODO : CoreType.h 경로 변경 요구
#include "Core/CoreMinimal.h"
#include "Render/Scene/RenderCommand.h"

#include "Render/Common/ViewTypes.h"


class FRenderBus
{
public:
	void Clear();
	void AddCommand(ERenderPass Pass, const FRenderCommand& InCommand);
	void AddCommand(ERenderPass Pass, FRenderCommand&& InCommand);
	const TArray<FRenderCommand>& GetCommands(ERenderPass Pass) const;

	// Getter,Setter
	void SetViewProjection(const FMatrix& InView, const FMatrix& InProj);
	void SetRenderSettings(const EViewMode NewViewMode, const FShowFlags NewShowFlags);

	const FMatrix& GetView() const { return View; }
	const FMatrix& GetProj() const { return Proj; }
	const FVector& GetCameraPosition() const { return CameraPosition;  }
	const FVector& GetCameraUp() const { return CameraUp; }
	const FVector& GetCameraRight() const { return CameraRight; }
	EViewMode GetViewMode() const { return ViewMode; }
	FShowFlags GetShowFlags() const { return ShowFlags; }
	const FVector& GetWireframeColor() const { return WireframeColor; }
	void SetWireframeColor(const FVector& InColor) { WireframeColor = InColor; }

private:
	TArray<FRenderCommand> PassQueues[(uint32)ERenderPass::MAX];

	FMatrix View;
	FMatrix Proj;
	FVector CameraPosition;
	FVector CameraRight;
	FVector CameraUp;

	//Editor Settings
	EViewMode ViewMode;
	FShowFlags ShowFlags;
	FVector WireframeColor = FVector(0.0f, 0.0f, 0.7f);
};

