//#pragma once
//
//#include "Object.h"
//#include "SceneComponent.h"
//#include "Math/Matrix.h"
//#include "Math/Vector.h"
//#include "ImGui/imgui.h"
//#include "Ray.h"
//
//enum class EProjectionType
//{
//	Perspective,
//	Orthogonal,
//	Isometric
//};
//
//const FVector UpVec(0.0f, 0.0f, 1.0f);
//const float Epsilon = 1e-6f;
//
//class UCameraComponent : public USceneComponent {
//	DECLARE_RTTI(UCameraComponent, USceneComponent)
//public:
//	float FOV{ 90.0f };
//	float AspectRatio{ 16.0f / 9.0f };
//	float NearPlane{ 0.01f };
//	float FarPlane{ 2000.0f };
//	float MoveSpeed{ 10.0f };
//	float ScreenWidth = 0.f;
//
//	float ScreenHeight = 0.f;
//	float RotateSensitivity = 1.0f;
//	bool IsOrthogonal{ false };
//	float OrthoHeight{ 10.0f };
//
//	float FocusLength{ 5.0f };
//	FVector FocusPoint;
//
//	//EProjectionType ProjectionType{ EProjectionType::Perspective };
//
//	FVector Direction{ FVector(1.0f, 0.0f, 0.0f) };
//	FVector SideDirection{ FVector(0.0f, 1.0f, 0.0f) };
//	FVector UpDirection{ FVector(0.0f, 0.0f, 1.0f) };
//
//	FMatrix View;
//	FMatrix Projection;
//	FMatrix ViewProjection;
//
//public:
//	UCameraComponent();
//	virtual ~UCameraComponent() override;
//
//	static UCameraComponent* GetMainCamera();
//
//	void SetRelativeLocation(const FVector& _input) override;
//	void Move(float DeltaTime);
//	FMatrix GetRotationMatrix();
//	void SetDirection(const float InputPitch, const float InputYaw,
//		const float InputRoll);
//	void HandleRotate(float DeltaTime);
//	void HandleOrbitRotate(float DeltaTime);
//	void SetFOV(const float NewFOV);
//	float GetFOV() const;
//	void OnResize(int NewWidth, int NewHeight);
//	float GetAspectRatio() const;
//	void SetOrthogonal(bool input);
//	//void ChangePerpective(EProjectionType NewType);
//	void Update(float deltaTime) override;
//
//	FMatrix GetViewMatrix() const;
//	void CalcProjectionMatrix();
//	FMatrix GetProjectionMatrix() const;
//	void CalcPerspectiveProjectionMatrix();
//	void CalcOrthogonalProjectionMatrix();
//	FMatrix GetViewProjectionMatrix() const;
//
//	FVector WorldToScreen(const FVector& worldPos) const;
//	Ray ScreenPointToRay(int x, int y);
//private:
//	ImGuiIO& io;
//	void HandleCameraMove(float deltaTime);
//	void UpdateCameraMatrices();
//	static UCameraComponent* mainCamera;
//};
