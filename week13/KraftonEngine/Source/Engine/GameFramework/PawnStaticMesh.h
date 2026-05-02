#pragma once

#include "GameFramework/Pawn.h"

class UStaticMeshComponent;

// ============================================================
// APawnStaticMesh — StaticMesh를 가진 Pawn (차량 등 베이스)
//
// AStaticMeshActor의 Pawn 버전. 게임플레이 코드에서 SpawnActor로 만들 때
// InitDefaultComponents로 메시 셋업.
// 직렬화/복제 시에는 컴포넌트가 씬에서 복원되며, PostDuplicate에서
// 캐시 포인터만 다시 잡는다.
// ============================================================
class APawnStaticMesh : public APawn
{
public:
	DECLARE_CLASS(APawnStaticMesh, APawn)

	APawnStaticMesh() = default;
	~APawnStaticMesh() override = default;

	void InitDefaultComponents(const FString& StaticMeshFileName = "Data/BasicShape/Cube.obj");
	void PostDuplicate() override;

	UStaticMeshComponent* GetStaticMeshComponent() const { return StaticMeshComponent; }

private:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
};
