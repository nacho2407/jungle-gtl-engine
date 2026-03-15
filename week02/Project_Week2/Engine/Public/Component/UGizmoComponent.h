#pragma once
#include "UObject.h"
#include "Types.h"
#include "USceneComponent.h"
#include "UGizmoComponent.h"
#include "FVector.h"

// gizmo 모드
enum class EGizmo : uint8 { // 값이 3개이므로 enum 크기를 1바이트로 고정. 메모리 최적화
	Location, Rotation, Scale
};

// 드래그 중인 축
enum class EGizmoAxis : uint8 {
	None, X, Y, Z
};

class UGizmoComponent : public USceneComponent
{
public:
	UGizmoComponent();

	void Update(float DeltaTime, const FVector& CameraLocation);
	
};
