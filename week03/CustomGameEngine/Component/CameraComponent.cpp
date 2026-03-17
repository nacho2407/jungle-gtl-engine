#include "CameraComponent.h"
#include "InputManager.h"
#include "Math/MathHelper.h"
#include "Math/Matrix.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"
#include "World.h"
#include "TimerManager.h"
#include <algorithm>

UCameraComponent::UCameraComponent() : io(ImGui::GetIO())
{}

UCameraComponent::~UCameraComponent() {}

void UCameraComponent::SetPosition(const FVector& _input) {
	if (!Position.IsNearlyEqual(_input, Epsilon)) {
		Position = _input;
	}
}

void UCameraComponent::Move(float DeltaTime) {
	if (InputManager::GetInstance().IsKeyHold('W') &&
		!InputManager::GetInstance().IsKeyHold('S')) {
		Position = Position + Direction * MoveSpeed * DeltaTime;
	}
	else if (!InputManager::GetInstance().IsKeyHold('W') &&
		InputManager::GetInstance().IsKeyHold('S')) {
		Position = Position - Direction * MoveSpeed * DeltaTime;
	}
	if (InputManager::GetInstance().IsKeyHold('E') &&
		!InputManager::GetInstance().IsKeyHold('Q')) {
		Position = Position + UpVec * MoveSpeed * DeltaTime;
	}
	else if (!InputManager::GetInstance().IsKeyHold('E') &&
		InputManager::GetInstance().IsKeyHold('Q')) {
		Position = Position - UpVec * MoveSpeed * DeltaTime;
	}
	if (!InputManager::GetInstance().IsKeyHold('D') &&
		InputManager::GetInstance().IsKeyHold('A')) {
		Position = Position + SideDirection * MoveSpeed * DeltaTime;
	}
	else if (InputManager::GetInstance().IsKeyHold('D') &&
		!InputManager::GetInstance().IsKeyHold('A')) {
		Position = Position - SideDirection * MoveSpeed * DeltaTime;
	}

}

void UCameraComponent::SetDirection(const float InputPitch, const float InputYaw,
	const float InputRoll) {
	if (std::abs(Pitch - InputPitch) > Epsilon) {
		Pitch = (std::max)(-89.9f, (std::min)(89.9f, InputPitch));
	}
	if (std::abs(Yaw - InputYaw) > Epsilon) {
		Yaw = InputYaw;
	}
	if (std::abs(Roll - InputRoll) > Epsilon) {
		Roll = InputRoll;
	}
}

void UCameraComponent::Rotate(float DeltaTime)
{
	Yaw += (InputManager::GetInstance().MouseDelta.x * RotateSensitivity * DeltaTime);
	Pitch += (InputManager::GetInstance().MouseDelta.y * RotateSensitivity * DeltaTime);
	Pitch = MathHelper::Clamp(Pitch, -89.9f, 89.9f);
}

void UCameraComponent::OrbitRotate(float DeltaTime)
{
	Yaw -= (InputManager::GetInstance().MouseDelta.x * RotateSensitivity * DeltaTime);
	Pitch -= (InputManager::GetInstance().MouseDelta.y * RotateSensitivity * DeltaTime);
	Pitch = MathHelper::Clamp(Pitch, -89.9f, 89.9f);

	FMatrix RotMatrix = FMatrix::RotationXMatrix(Roll) *
		FMatrix::RotationYMatrix(Pitch) *
		FMatrix::RotationZMatrix(Yaw);

	FVector ForwardBase = FVector::Forward();
	FVector ForwardVec = RotMatrix.TransformVector(ForwardBase).Normalize();

	Position = FocusPoint - ForwardVec * FocusLength;
}

void UCameraComponent::SetFOV(const float NewFOV)
{
	if (std::abs(FOV - NewFOV) > Epsilon) {
		FOV = std::clamp(NewFOV, 10.0f, 170.0f);
	}
}
float UCameraComponent::GetFOV() const
{
	return FOV;
}

void UCameraComponent::OnResize(int NewWidth, int NewHeight) {
	if (NewHeight == 0) return;
	ScreenWidth = static_cast<float>(NewWidth);
	ScreenHeight = static_cast<float>(NewHeight);
	AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
}

float UCameraComponent::GetAspectRatio() const
{
	return AspectRatio;
}

void UCameraComponent::SetOrthogonal(bool input)
{
	IsOrthogonal = input;
}

//void UCamera::ChangePerpective(EProjectionType NewType)
//{
//	ProjectionType = NewType;
//	bIsDirty = true;
//}

void UCameraComponent::Update(float deltaTime) {
	HandleCameraMove(deltaTime);
	UpdateCameraMatrices();
}

void UCameraComponent::HandleCameraMove(float deltaTime)
{
	if (!io.WantCaptureKeyboard && !io.WantCaptureMouse)
	{
		if (InputManager::GetInstance().IsKeyHold(VK_RBUTTON))
		{
			Move(deltaTime);
			Rotate(deltaTime);
		}
		else if (InputManager::GetInstance().IsKeyHold(VK_LBUTTON)
			&& InputManager::GetInstance().IsKeyHold(VK_MENU))
		{
			OrbitRotate(deltaTime);
		}
		if (InputManager::GetInstance().IsKeyHold('F')
			&& GetWorld().GetActiveScene()->IsCompSelected)
		{
			// SetPosition(EditorInst->GetSelectedComponent()->GetRelativeLocation() - Direction * FocusLength);
		}
	}
}

void UCameraComponent::UpdateCameraMatrices()
{
	FMatrix RotMatrix = FMatrix::RotationXMatrix(Roll) *
		FMatrix::RotationYMatrix(Pitch) *
		FMatrix::RotationZMatrix(Yaw);

	FVector ForwardBase = FVector::Forward();
	Direction = RotMatrix.TransformVector(ForwardBase).Normalize();

	SideDirection = Direction.Cross(FVector::Up());
	UpDirection = SideDirection.Cross(Direction);

	FocusPoint = Position + Direction * FocusLength;

	View = FMatrix::ViewMatrix(Position, FocusPoint, UpDirection);
	CalcProjectionMatrix();
	ViewProjection = View * Projection;
}

void UCameraComponent::CalcViewMatrix()
{
	FMatrix InvT;
	InvT[3][2] = -Position.x;
	InvT[3][0] = -Position.y;
	InvT[3][1] = -Position.z;

	FMatrix InvR = FMatrix::RotationZMatrix(-Roll) *
		FMatrix::RotationYMatrix(-Yaw) *
		FMatrix::RotationXMatrix(-Pitch);

	View = InvT * InvR;
}


FMatrix UCameraComponent::GetViewMatrix() const
{
	return View;
}

//void UCamera::CalcProjectionMatrix()
//{
//	if (ProjectionType == EProjectionType::Perspective)
//		CalcPerspectiveProjectionMatrix();
//	else
//		CalcOrthogonalProjectionMatrix();
//}

void UCameraComponent::CalcProjectionMatrix()
{
	if (!IsOrthogonal)
		CalcPerspectiveProjectionMatrix();
	else
		CalcOrthogonalProjectionMatrix();
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	return Projection;
}

void UCameraComponent::CalcPerspectiveProjectionMatrix()
{
	FMatrix M = FMatrix::ZeroMatrix();

	float rad = FMatrix::DegToRad(FOV);
	float w = 1.0f / std::tan(rad * 0.5f);
	float h = w * AspectRatio;

	float q = FarPlane / (FarPlane - NearPlane);
	float b = (-NearPlane * FarPlane) / (FarPlane - NearPlane);

	M[0][0] = w;
	M[1][1] = h;
	M[2][2] = q;
	M[3][2] = b;
	M[2][3] = 1.0f;

	Projection = M;
}

void UCameraComponent::CalcOrthogonalProjectionMatrix()
{
	FMatrix M;

	float OrthoWidth = OrthoHeight * AspectRatio;

	float w = 2.0f / OrthoWidth;
	float h = 2.0f / OrthoHeight;

	float q = 1.0f / (FarPlane - NearPlane);
	float b = -NearPlane / (FarPlane - NearPlane);

	M[0][0] = w;
	M[1][1] = h;
	M[2][2] = q;
	M[3][2] = b;
	M[3][3] = 1.0f;

	Projection = M;
}

FMatrix UCameraComponent::GetViewProjectionMatrix() const { return ViewProjection; }

REGISTER_CLASS(UCameraComponent);