#include "USceneComponent.h"
#include "ImGui/imgui.h"

USceneComponent::USceneComponent()
{
	RelativeLocation = FVector(0.0f, 0.0f, 0.0f);
	RelativeRotation = FVector(0.0f, 0.0f, 0.0f);
	RelativeScale3D = FVector(1.0f, 1.0f, 1.0f);
}

USceneComponent::~USceneComponent()
{

}

FMatrix USceneComponent::GetRelativeMatrix() const
{
	FMatrix S = FMatrix::ScaleMatrix(RelativeScale3D.x, RelativeScale3D.y, RelativeScale3D.z);
	FMatrix R = FMatrix::EulerRotationMatrix(RelativeRotation.x, RelativeRotation.y, RelativeRotation.z);
	FMatrix T = FMatrix::TranslationMatrix(RelativeLocation.x, RelativeLocation.y, RelativeLocation.z);

	return S * R * T;
}

json::JSON USceneComponent::Serialize()
{
	json::JSON jsonObj;
	jsonObj["Location"] = RelativeLocation.Serialize();
	jsonObj["Rotation"] = RelativeRotation.Serialize();
	jsonObj["Scale"] = RelativeScale3D.Serialize();
	return jsonObj;
}

void USceneComponent::Deserialize(json::JSON jsonObj)
{
	RelativeLocation.Deserialize(jsonObj["Location"]);
	RelativeRotation.Deserialize(jsonObj["Rotation"]);
	RelativeScale3D.Deserialize(jsonObj["Scale"]);
}

void USceneComponent::DrawProperties()
{
	ImGui::InputFloat3("Location", &RelativeLocation.x);
	ImGui::InputFloat3("Rotation", &RelativeRotation.x);
	ImGui::InputFloat3("Scale", &RelativeScale3D.x);
}
