#include "UCamera.h"

UCamera::~UCamera() {}

void UCamera::SetPosition(const FVector& _input) {
	if (!Position.IsNearlyEqual(_input, Epsilon)) {
		Position = _input;
		IsChanged = true;
	}
}

void UCamera::Move() {
	if (InputManager::GetInstance().IsKeyHold('W') &&
		!InputManager::GetInstance().IsKeyHold('S')) {
		Position = Position + Direction * MoveSpeed;
		IsChanged = true;
	}
	else if (!InputManager::GetInstance().IsKeyHold('W') &&
		InputManager::GetInstance().IsKeyHold('S')) {
		Position = Position - Direction * MoveSpeed;
		IsChanged = true;
	}
	if (InputManager::GetInstance().IsKeyHold('E') &&
		!InputManager::GetInstance().IsKeyHold('Q')) {
		Position = Position + UpVec * MoveSpeed;
		IsChanged = true;
	}
	else if (!InputManager::GetInstance().IsKeyHold('E') &&
		InputManager::GetInstance().IsKeyHold('Q')) {
		Position = Position - UpVec * MoveSpeed;
		IsChanged = true;
	}
	if (!InputManager::GetInstance().IsKeyHold('D') &&
		InputManager::GetInstance().IsKeyHold('A')) {
		Position = Position + SideDirection * MoveSpeed;
		IsChanged = true;
	}
	else if (InputManager::GetInstance().IsKeyHold('D') &&
		!InputManager::GetInstance().IsKeyHold('A')) {
		Position = Position - SideDirection * MoveSpeed;
		IsChanged = true;
	}
	
}

void UCamera::SetDirection(const float InputPitch, const float InputYaw,
	const float InputRoll) {
	if (std::abs(Pitch - InputPitch) > Epsilon) {
		Pitch = (std::max)(-89.9f, (std::min)(89.9f, InputPitch));
		IsChanged = true;
	}
	if (std::abs(Yaw - InputYaw) > Epsilon) {
		Yaw = InputYaw;
		IsChanged = true;
	}
	if (std::abs(Roll - InputRoll) > Epsilon) {
		Roll = InputRoll;
		IsChanged = true;
	}
}

void UCamera::Rotate()
{
	if (InputManager::GetInstance().IsKeyHold(VK_RBUTTON))
	{
		Yaw += InputManager::GetInstance().MouseDelta.x * RotateSensitivity;
		Pitch -= InputManager::GetInstance().MouseDelta.y * RotateSensitivity;
		IsChanged = true;
	}
}

void UCamera::SetFOV(const float NewFOV)
{
	if (std::abs(FOV - NewFOV) > Epsilon) {
		FOV = std::clamp(NewFOV, 10.0f, 170.0f);
		IsChanged = true;
	}
}
float UCamera::GetFOV() const
{
	return FOV;
}

void UCamera::OnResize(int NewWidth, int NewHeight) {
	if (NewHeight == 0) return;
	AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
	IsChanged = true;
}

float UCamera::GetAspectRatio() const
{
	return AspectRatio;
}

void UCamera::update() {
	if (!IsChanged) return;

	View = GetViewMatrix();
	Projection = GetProjectionMatrix();
	ViewProjection = View * Projection;

	float p = FMatrix::DegToRad(Pitch);
	float y = FMatrix::DegToRad(Yaw);

	Direction.x = std::cos(p) * std::sin(y);
	Direction.y = std::sin(p);
	Direction.z = std::cos(p) * std::cos(y);

	Direction = Direction.Normalize();
	SideDirection = Direction.Cross(UpVec);
	UpDirection = SideDirection.Cross(Direction);

	IsChanged = false;
}

FMatrix UCamera::GetViewMatrix() const {
	FMatrix InvT;
	InvT[3][0] = -Position.x;
	InvT[3][1] = -Position.y;
	InvT[3][2] = -Position.z;

	FMatrix InvR = FMatrix::RotationZMatrix(-Roll) *
		FMatrix::RotationYMatrix(-Yaw) *
		FMatrix::RotationXMatrix(-Pitch);
	return InvT * InvR;
}

FMatrix UCamera::GetProjectionMatrix() const {
	FMatrix M = FMatrix::ZeroMatrix();

	float rad = FMatrix::DegToRad(FOV);
	float h = 1.0f / std::tan(rad * 0.5f);
	float w = h / AspectRatio;

	float q = FarPlane / (FarPlane - NearPlane);
	float b = (-NearPlane * FarPlane) / (FarPlane - NearPlane);

	M[0][0] = w;
	M[1][1] = h;
	M[2][2] = q;
	M[3][2] = b;
	M[2][3] = 1.0f;

	return M;
}

FMatrix UCamera::GetViewProjectionMatrix() const { return ViewProjection; }

REGISTER_CLASS(UCamera);