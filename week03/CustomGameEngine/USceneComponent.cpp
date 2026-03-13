#include "USceneComponent.h"

USceneComponent::USceneComponent()
{
	RelativeLocation = FVector(0.0f, 0.0f, 0.0f);
	RelativeRotation = FVector(0.0f, 0.0f, 0.0f);
	RelativeScale3D = FVector(1.0f, 1.0f, 1.0f);
}

USceneComponent::~USceneComponent()
{

}
