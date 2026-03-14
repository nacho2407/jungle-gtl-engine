#pragma once
#include "USceneComponent.h"
#include "FMatrix.h"
#include "Types.h"

class UCameraComponent : public USceneComponent
{
public:
	UCameraComponent();

	void UpdateMatrices();

	FMatrix GetViewMatrix() const { return ViewMatrix; }
	FMatrix GetProjectionMatrix() const { return ProjectionMatrix; }

	void UpdateAspectRatio(uint32 Width, uint32 Height); // 창 크기 변경시에 출력

public:
	float FOV; // Field of View 카메라 시야각
	float AspectRatio;
	float NearPlane;
	float FarPlane;

private:
	FMatrix ViewMatrix;
	FMatrix ProjectionMatrix;
};