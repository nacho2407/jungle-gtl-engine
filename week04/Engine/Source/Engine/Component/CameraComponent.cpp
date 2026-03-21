//#include "CameraComponent.h"
//#include "InputManager.h"
//#include "Math/MathHelper.h"
//#include "Math/Matrix.h"
//#include "Math/Quaternion.h"
//#include "Math/Vector.h"
//#include "Math/Vector4.h"
//#include "World.h"
//#include "TimerManager.h"
//#include "Logger.h"
//#include <algorithm>
//
//UCameraComponent* UCameraComponent::mainCamera = nullptr;
//
//UCameraComponent::UCameraComponent() : io(ImGui::GetIO())
//{
//	if (mainCamera != nullptr)
//		UE_LOG("2 or more camera exist at the same time!\n");
//	mainCamera = this;
//}
//
//UCameraComponent::~UCameraComponent() {
//	if (mainCamera == this)
//		mainCamera = nullptr;
//}
//
//UCameraComponent* UCameraComponent::GetMainCamera()
//{
//	return mainCamera;
//}
//
//void UCameraComponent::SetRelativeLocation(const FVector& _input) {
//	if (!GetRelativeLocation().IsNearlyEqual(_input, Epsilon)) {
//		USceneComponent::SetRelativeLocation(_input);
//	}
//}
//
//void UCameraComponent::Move(float DeltaTime) {
//	FVector position = GetRelativeLocation();
//
//	if (InputManager::GetInstance().IsKeyHold('W') &&
//		!InputManager::GetInstance().IsKeyHold('S')) {
//		position = position + Direction * MoveSpeed * DeltaTime;
//	}
//	else if (!InputManager::GetInstance().IsKeyHold('W') &&
//		InputManager::GetInstance().IsKeyHold('S')) {
//		position = position - Direction * MoveSpeed * DeltaTime;
//	}
//	if (InputManager::GetInstance().IsKeyHold('E') &&
//		!InputManager::GetInstance().IsKeyHold('Q')) {
//		position = position + UpVec * MoveSpeed * DeltaTime;
//	}
//	else if (!InputManager::GetInstance().IsKeyHold('E') &&
//		InputManager::GetInstance().IsKeyHold('Q')) {
//		position = position - UpVec * MoveSpeed * DeltaTime;
//	}
//	if (!InputManager::GetInstance().IsKeyHold('D') &&
//		InputManager::GetInstance().IsKeyHold('A')) {
//		position = position - SideDirection * MoveSpeed * DeltaTime;
//	}
//	else if (InputManager::GetInstance().IsKeyHold('D') &&
//		!InputManager::GetInstance().IsKeyHold('A')) {
//		position = position + SideDirection * MoveSpeed * DeltaTime;
//	}
//
//	SetRelativeLocation(position);
//}
//
//FMatrix GetRotationMatrix(float roll, float pitch, float yaw)
//{
//	return FMatrix::RotationXMatrix(roll)*
//		FMatrix::RotationYMatrix(pitch)*
//		FMatrix::RotationZMatrix(yaw);
//}
//
//FMatrix GetRotationMatrix(FRotator eulerAngle)
//{
//	return eulerAngle.ToMatrix();
//}
//
//FMatrix UCameraComponent::GetRotationMatrix()
//{
//	return GetRelativeRotation().ToMatrix();
//}
//
//void UCameraComponent::SetDirection(const float InputPitch, const float InputYaw, const float InputRoll)
//{ 	
//	FRotator eulerAngle = GetRelativeRotation();
//
//	if (std::abs(eulerAngle.Roll - InputRoll) > Epsilon) {
//		eulerAngle.Roll = InputRoll;
//	}
//	if (std::abs(eulerAngle.Pitch - InputPitch) > Epsilon) {
//		eulerAngle.Pitch = (std::max)(-89.9f, (std::min)(89.9f, InputPitch));
//	}
//	if (std::abs(eulerAngle.Yaw - InputYaw) > Epsilon) {
//		eulerAngle.Yaw = InputYaw;
//	}
//	SetRelativeRotation(eulerAngle);
//}
//
//void UCameraComponent::HandleRotate(float DeltaTime)
//{
//	FRotator eulerAngle = GetRelativeRotation();
//
//	float newPitch = eulerAngle.Pitch + (InputManager::GetInstance().MouseDelta.y * RotateSensitivity);
//	newPitch = MathHelper::Clamp(newPitch, -89.9f, 89.9f);
//	eulerAngle.Pitch = newPitch;
//	eulerAngle.Yaw = eulerAngle.Yaw + (InputManager::GetInstance().MouseDelta.x * RotateSensitivity);
//
//	SetRelativeRotation(eulerAngle);
//}
//
//void UCameraComponent::HandleOrbitRotate(float DeltaTime)
//{
//
//	FRotator eulerAngle = GetRelativeRotation();
//
//	float newPitch = eulerAngle.Pitch + (InputManager::GetInstance().MouseDelta.y * RotateSensitivity);
//	newPitch = MathHelper::Clamp(newPitch, -89.9f, 89.9f);
//	eulerAngle.Pitch = newPitch;
//	eulerAngle.Yaw = eulerAngle.Yaw + (InputManager::GetInstance().MouseDelta.x * RotateSensitivity);
//
//	SetRelativeRotation(eulerAngle);
//
//	FMatrix RotMatrix = GetRotationMatrix();
//	FVector ForwardBase = FVector::Forward();
//	FVector ForwardVec = RotMatrix.TransformVector(ForwardBase).Normalize();
//
//	SetRelativeLocation(FocusPoint - ForwardVec * FocusLength);
//}
//
//void UCameraComponent::SetFOV(const float NewFOV)
//{
//	if (std::abs(FOV - NewFOV) > Epsilon) {
//		FOV = std::clamp(NewFOV, 10.0f, 170.0f);
//	}
//}
//float UCameraComponent::GetFOV() const
//{
//	return FOV;
//}
//
//void UCameraComponent::OnResize(int NewWidth, int NewHeight) {
//	if (NewHeight == 0) return;
//	ScreenWidth = static_cast<float>(NewWidth);
//	ScreenHeight = static_cast<float>(NewHeight);
//	AspectRatio = static_cast<float>(NewWidth) / static_cast<float>(NewHeight);
//}
//
//float UCameraComponent::GetAspectRatio() const
//{
//	return AspectRatio;
//}
//
//void UCameraComponent::SetOrthogonal(bool input)
//{
//	IsOrthogonal = input;
//}
//
//void UCamera::ChangePerpective(EProjectionType NewType)
//{
//	ProjectionType = NewType;
//	bIsDirty = true;
//}
//
//void UCameraComponent::Update(float deltaTime) {
//	HandleCameraMove(deltaTime);
//	UpdateCameraMatrices();
//}
//
//void UCameraComponent::HandleCameraMove(float deltaTime)
//{
//	if (!io.WantCaptureKeyboard && !io.WantCaptureMouse)
//	{
//		if (InputManager::GetInstance().IsKeyHold(VK_RBUTTON))
//		{
//			Move(deltaTime);
//			HandleRotate(deltaTime);
//		}
//		else if (InputManager::GetInstance().IsKeyHold(VK_LBUTTON)
//			&& InputManager::GetInstance().IsKeyHold(VK_MENU))
//		{
//			HandleOrbitRotate(deltaTime);
//		}
//		if (InputManager::GetInstance().IsKeyHold('F')
//			&& GetWorld().GetActiveScene()->IsCompSelected)
//		{
//			 TODO: 오브젝트 포커싱 재구현
//			 SetPosition(EditorInst->GetSelectedComponent()->GetRelativeLocation() - Direction * FocusLength);
//		}
//	}
//}
//
//void UCameraComponent::UpdateCameraMatrices()
//{
//	FMatrix RotMatrix = GetRotationMatrix();
//
//	FVector ForwardBase = FVector::Forward();
//	Direction = RotMatrix.TransformVector(ForwardBase).Normalize();
//
//	SideDirection = FVector::Up().Cross(Direction);
//	UpDirection = Direction.Cross(SideDirection);
//
//	auto position = GetRelativeLocation();
//	FocusPoint = position + Direction * FocusLength;
//	View = FMatrix::ViewMatrix(position, FocusPoint, UpDirection);
//	CalcProjectionMatrix();
//	ViewProjection = View * Projection;
//}
//
//FMatrix UCameraComponent::GetViewMatrix() const
//{
//	return View;
//}
//
//void UCamera::CalcProjectionMatrix()
//{
//	if (ProjectionType == EProjectionType::Perspective)
//		CalcPerspectiveProjectionMatrix();
//	else
//		CalcOrthogonalProjectionMatrix();
//}
//
//void UCameraComponent::CalcProjectionMatrix()
//{
//	if (!IsOrthogonal)
//		CalcPerspectiveProjectionMatrix();
//	else
//		CalcOrthogonalProjectionMatrix();
//}
//
//FMatrix UCameraComponent::GetProjectionMatrix() const
//{
//	return Projection;
//}
//
//void UCameraComponent::CalcPerspectiveProjectionMatrix()
//{
//	FMatrix M = FMatrix::ZeroMatrix();
//
//	float rad = FMatrix::DegToRad(FOV);
//	float w = 1.0f / std::tan(rad * 0.5f);
//	float h = w * AspectRatio;
//
//	float q = -NearPlane / (FarPlane - NearPlane);
//	float b = (NearPlane * FarPlane) / (FarPlane - NearPlane);
//
//	M[0][0] = w;
//	M[1][1] = h;
//	M[2][2] = q;
//	M[3][2] = b;
//	M[2][3] = 1.0f;
//
//	Projection = M;
//}
//
//void UCameraComponent::CalcOrthogonalProjectionMatrix()
//{
//	FMatrix M;
//
//	float OrthoWidth = OrthoHeight * AspectRatio;
//
//	float w = 2.0f / OrthoWidth;
//	float h = 2.0f / OrthoHeight;
//
//	float q = -1.0f / (FarPlane - NearPlane);
//	float b = FarPlane / (FarPlane - NearPlane);
//
//	M[0][0] = w;
//	M[1][1] = h;
//	M[2][2] = q;
//	M[3][2] = b;
//	M[3][3] = 1.0f;
//
//	Projection = M;
//}
//
//FMatrix UCameraComponent::GetViewProjectionMatrix() const { return ViewProjection; }
//
//FVector UCameraComponent::WorldToScreen(const FVector& worldPos) const
//{
//	FVector clipSpacePos = ViewProjection.TransformPoint(worldPos);
//	if (clipSpacePos.z == 0.0f) return FVector(-1.0f, -1.0f, -1.0f);
//
//	FVector ndcSpacePos = clipSpacePos / clipSpacePos.z;
//	float screenX = (ndcSpacePos.x + 1.0f) * 0.5f * ScreenWidth;
//	float screenY = (1.0f - (ndcSpacePos.y + 1.0f) * 0.5f) * ScreenHeight;
//
//	return FVector(screenX, screenY, ndcSpacePos.z);
//}
//
//Ray UCameraComponent::ScreenPointToRay(int x, int y)
//{
//	Screen Space -> NDC Space
//	float ndcX = (2.0f * (static_cast<float>(x) + 0.5f)) / ScreenWidth - 1.0f;
//	float ndcY = 1.0f - (2.0f * (static_cast<float>(y) + 0.5f)) / ScreenHeight;
//	NDC -> View Space
//	FVector4 Near = FVector4(ndcX, ndcY, 1.f, 1.f);
//	FVector4 Far = FVector4(ndcX, ndcY, 0.f, 1.f);
//	FMatrix InvProjMat = GetProjectionMatrix().Inverse();
//	FVector4 NearView = Near * InvProjMat;
//	FVector4 FarView = Far * InvProjMat;
//	NearView = NearView * (1.0f / NearView.w);
//	FarView = FarView * (1.0f / FarView.w);
//	View Space -> World Space
//	FMatrix InvView = GetViewMatrix().Inverse();
//	FVector4 NearWorld4 = NearView * InvView;
//	FVector4 FarWorld4 = FarView * InvView;
//	FVector NearWorld = FVector(NearWorld4.x, NearWorld4.y, NearWorld4.z);
//	FVector FarWorld = FVector(FarWorld4.x, FarWorld4.y, FarWorld4.z);
//	return Ray(NearWorld, FarWorld - NearWorld);
//}
//
//REGISTER_CLASS(UCameraComponent);