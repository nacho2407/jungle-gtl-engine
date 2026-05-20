#pragma once

#include "Object/Object.h"
#include "Core/CoreTypes.h"

#include "Source/Engine/Animation/AnimGraphAsset.generated.h"

class FArchive;

// AnimGraph (시각 노드 그래프) 자산의 깡통.
// 이후 단계에서 Nodes / Pins / Links / Variables 등을 추가하고
// 컴파일 결과로 UAnimInstance::SetRootNode 에 박힐 트리를 생성한다.
//
// 라이프사이클:
//   - ContentBrowser 의 자산 더블클릭 또는 메뉴 진입점으로
//     UAssetEditorManager 가 FAnimGraphEditorWidget 을 띄워 편집.
//   - Save 시 .AnimGraph 패키지로 직렬화 (Manager 는 후속 단계).
UCLASS()
class UAnimGraphAsset : public UObject
{
public:
	GENERATED_BODY()
	UAnimGraphAsset() = default;
	~UAnimGraphAsset() override = default;

	void           SetSourcePath(const FString& InPath) { SourcePath = InPath; }
	const FString& GetSourcePath() const                { return SourcePath; }

	// Placeholder. Nodes/Pins/Links 추가 시 본문 채움.
	void Serialize(FArchive& Ar) override;

private:
	FString SourcePath;
};
