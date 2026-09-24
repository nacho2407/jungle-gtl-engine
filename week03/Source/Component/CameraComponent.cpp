#include "CameraComponent.h"
#include "EngineCore/InputManager.h"
#include "Math/MathHelper.h"
#include "Math/Matrix.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "World.h"
#include "EngineCore/TimerManager.h"
#include "EngineCore/Logger.h"
#include <algorithm>

UCameraComponent* UCameraComponent::mainCamera = nullptr;

UCameraComponent::UCameraComponent() : io(ImGui::GetIO())
{
	if (mainCamera != nullptr)
		UE_LOG("2 or more camera exist at the same time!\n");
	mainCamera = this;
}

UCameraComponent::~UCameraComponent() {
	if (mainCamera == this)
		mainCamera = nullptr;
}

UCameraComponent* UCameraComponent::GetMainCamera()
{
	return mainCamera;
}

void UCameraComponent::SetRelativeLocation(const FVector& _input) {
	if (!GetRelativeLocation().IsNearlyEqual(_input, Epsilon)) {
		USceneComponent::SetRelativeLocation(_input);
	}
}

void UCameraComponent::Move(float DeltaTime) {
	FVector position = GetRelativeLocation();

	if (InputManager::GetInstance().IsKeyHold('W') &&
		!InputManager::GetInstance().IsKeyHold('S')) {
		position = position + Direction * MoveSpeed * DeltaTime;
	}
	else if (!InputManager::GetInstance().IsKeyHold('W') &&
		InputManager::GetInstance().IsKeyHold('S')) {
		position = position - Direction * MoveSpeed * DeltaTime;
	}
	if (InputManager::GetInstance().IsKeyHold('E') &&
		!InputManager::GetInstance().IsKeyHold('Q')) {
		position = position + UpVec * MoveSpeed * DeltaTime;
	}
	else if (!InputManager::GetInstance().IsKeyHold('E') &&
		InputManager::GetInstance().IsKeyHold('Q')) {
		position = position - UpVec * MoveSpeed * DeltaTime;
	}
	if (!InputManager::GetInstance().IsKeyHold('D') &&
		InputManager::GetInstance().IsKeyHold('A')) {
		position = position - SideDirection * MoveSpeed * DeltaTime;
	}
	else if (InputManager::GetInstance().IsKeyHold('D') &&
		!InputManager::GetInstance().IsKeyHold('A')) {
		position = position + SideDirection * MoveSpeed * DeltaTime;
	}

	SetRelativeLocation(position);
}

FMatrix GetRotationMatrix(float roll, float pitch, float yaw)
{
	return FMatrix::RotationXMatrix(roll) *
		FMatrix::RotationYMatrix(pitch) *
		FMatrix::RotationZMatrix(yaw);
}

FMatrix GetRotationMatrix(FRotator eulerAngle)
{
	return eulerAngle.ToMatrix();
}

FMatrix UCameraComponent::GetRotationMatrix()
{
	return GetRelativeRotation().ToMatrix();
}

void UCameraComponent::SetDirection(const float InputPitch, const float InputYaw, const float InputRoll)
{
	FRotator eulerAngle = GetRelativeRotation();

	if (std::abs(eulerAngle.Roll - InputRoll) > Epsilon) {
		eulerAngle.Roll = InputRoll;
	}
	if (std::abs(eulerAngle.Pitch - InputPitch) > Epsilon) {
		eulerAngle.Pitch = (std::max)(-89.9f, (std::min)(89.9f, InputPitch));
	}
	if (std::abs(eulerAngle.Yaw - InputYaw) > Epsilon) {
		eulerAngle.Yaw = InputYaw;
	}
	SetRelativeRotation(eulerAngle);
}

void UCameraComponent::HandleRotate()
{
	FRotator eulerAngle = GetRelativeRotation();
	FVector MouseDelta = InputManager::GetInstance().MouseDelta;
	float rotateFactor = RotateSensitivity* 0.1f;
	float newPitch = eulerAngle.Pitch + (MouseDelta.y * rotateFactor);
	newPitch = MathHelper::Clamp(newPitch, -89.9f, 89.9f);
	eulerAngle.Pitch = newPitch;
	eulerAngle.Yaw = eulerAngle.Yaw + (MouseDelta.x * rotateFactor);

	SetRelativeRotation(eulerAngle);
}

void UCameraComponent::HandlePanning(float DeltaTime)
{
	float panFactor = MoveSpeed * 1.0f * DeltaTime;
	FVector MouseDelta = InputManager::GetInstance().MouseDelta;
	FVector prevLocation = GetRelativeLocation();
	prevLocation = prevLocation - SideDirection * MouseDelta.x * panFactor;
	prevLocation = prevLocation + UpDirection * MouseDelta.y * panFactor;
	SetRelativeLocation(prevLocation);
}

void UCameraComponent::HandleOrbitRotate(float DeltaTime)
{

	FRotator eulerAngle = GetRelativeRotation();

	float newPitch = eulerAngle.Pitch + (InputManager::GetInstance().MouseDelta.y * RotateSensitivity);
	newPitch = MathHelper::Clamp(newPitch, -89.9f, 89.9f);
	eulerAngle.Pitch = newPitch;
	eulerAngle.Yaw = eulerAngle.Yaw + (InputManager::GetInstance().MouseDelta.x * RotateSensitivity);

	SetRelativeRotation(eulerAngle);

	FMatrix RotMatrix = GetRotationMatrix();
	FVector ForwardBase = FVector::Forward();
	FVector ForwardVec = RotMatrix.TransformVector(ForwardBase).Normalize();

	SetRelativeLocation(FocusPoint - ForwardVec * FocusLength);
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
	HandleCameraMouseInput(deltaTime);
	UpdateCameraMatrices();
}

// 마우스 휠을 이용한 발줌입니다.
void UCameraComponent::HandleCameraMouseInput(float deltaTime)
{
	if (!io.WantCaptureKeyboard && !io.WantCaptureMouse)
	{
		float wheelDelta = InputManager::GetInstance().MouseWheel;
		if (wheelDelta != 0.0f)
		{
			FVector Mouse = InputManager::GetInstance().MousePos;
			Ray mouseRay = ScreenPointToRay(static_cast<int32>(Mouse.x), static_cast<int32>(Mouse.y));

			FVector position = GetRelativeLocation();
			// 마우스 커서가 가리키는 월드 방향으로 이동
			position = position + mouseRay.Direction * wheelDelta * MoveSpeed * 0.5f;
			SetRelativeLocation(position);
		}

		if(InputManager::GetInstance().IsKeyHold(VK_MBUTTON))
		{
			HandlePanning(deltaTime);
		}

		if (InputManager::GetInstance().IsKeyHold(VK_RBUTTON))
		{
			Move(deltaTime);
			HandleRotate();
		}
		else if (InputManager::GetInstance().IsKeyHold(VK_LBUTTON)
			&& InputManager::GetInstance().IsKeyHold(VK_MENU))
		{
			HandleOrbitRotate(deltaTime);
		}
		UScene* activeScene = GetWorld().GetSceneManager()->GetActiveScene();
		if (InputManager::GetInstance().IsKeyHold('F')
			&& activeScene != nullptr
			&& activeScene->IsCompSelected)
		{
			// TODO: 오브젝트 포커싱 재구현
			// SetPosition(EditorInst->GetSelectedComponent()->GetRelativeLocation() - Direction * FocusLength);
		}
	}
}

void UCameraComponent::UpdateCameraMatrices()
{
	FMatrix RotMatrix = GetRotationMatrix();

	FVector ForwardBase = FVector::Forward();
	Direction = RotMatrix.TransformVector(ForwardBase).Normalize();

	// Direction이 world up과 거의 평행해지는 경우 대비
	const float UpDot = std::abs(Direction.Dot(FVector::Up()));
	if (UpDot > 1.0f - Epsilon)
	{
		// 안전장치...
		SideDirection = FVector::Right();
	}
	else
	{
		SideDirection = FVector::Up().Cross(Direction).Normalize();
	}

	UpDirection = Direction.Cross(SideDirection);

	auto position = GetRelativeLocation();
	FocusPoint = position + Direction * FocusLength;
	View = FMatrix::ViewMatrix(position, FocusPoint, UpDirection);
	CalcProjectionMatrix();
	ViewProjection = View * Projection;
}

FMatrix UCameraComponent::GetViewMatrix() const
{
	return View;
}

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
	Projection = FMatrix::PerspectiveMatrix(FOV, AspectRatio, NearPlane, FarPlane);
}

void UCameraComponent::CalcOrthogonalProjectionMatrix()
{
	FMatrix M;

	float OrthoWidth = OrthoHeight * AspectRatio;

	float w = 2.0f / OrthoWidth;
	float h = 2.0f / OrthoHeight;

	float q = -1.0f / (FarPlane - NearPlane);
	float b = FarPlane / (FarPlane - NearPlane);

	M[0][0] = w;
	M[1][1] = h;
	M[2][2] = q;
	M[3][2] = b;
	M[3][3] = 1.0f;

	Projection = M;
}

FMatrix UCameraComponent::GetViewProjectionMatrix() const {
	return ViewProjection;
}

FVector UCameraComponent::WorldToScreen(const FVector& worldPos) const
{
	if (ScreenWidth <= 0.0f || ScreenHeight <= 0.0f)
	{
		return FVector(-1.0f, -1.0f, -1.0f);
	}

	const FVector4 clip = FVector4(worldPos.x, worldPos.y, worldPos.z, 1.0f) * ViewProjection;

	if (std::abs(clip.w) < Epsilon || clip.w <= 0.0f)
	{
		return FVector(-1.0f, -1.0f, -1.0f);
	}

	const float invW = 1.0f / clip.w;
	const float ndcX = clip.x * invW;
	const float ndcY = clip.y * invW;
	const float ndcZ = clip.z * invW;

	const float screenX = (ndcX + 1.0f) * 0.5f * ScreenWidth;
	const float screenY = (1.0f - (ndcY + 1.0f) * 0.5f) * ScreenHeight;

	return FVector(screenX, screenY, ndcZ);
}

Ray UCameraComponent::ScreenPointToRay(int x, int y)
{
	//Screen Space -> NDC Space
	float ndcX = (2.0f * (static_cast<float>(x) + 0.5f)) / ScreenWidth - 1.0f;
	float ndcY = 1.0f - (2.0f * (static_cast<float>(y) + 0.5f)) / ScreenHeight;
	//NDC -> View Space
	FVector4 Near = FVector4(ndcX, ndcY, 1.f, 1.f);
	FVector4 Far = FVector4(ndcX, ndcY, 0.f, 1.f);
	FMatrix InvProjMat = GetProjectionMatrix().Inverse();
	FVector4 NearView = Near * InvProjMat;
	FVector4 FarView = Far * InvProjMat;
	NearView = NearView * (1.0f / NearView.w);
	FarView = FarView * (1.0f / FarView.w);
	//View Space -> World Space
	FMatrix InvView = GetViewMatrix().Inverse();
	FVector4 NearWorld4 = NearView * InvView;
	FVector4 FarWorld4 = FarView * InvView;
	FVector NearWorld = FVector(NearWorld4.x, NearWorld4.y, NearWorld4.z);
	FVector FarWorld = FVector(FarWorld4.x, FarWorld4.y, FarWorld4.z);
	return Ray(NearWorld, FarWorld - NearWorld);
}

REGISTER_CLASS(UCameraComponent);
