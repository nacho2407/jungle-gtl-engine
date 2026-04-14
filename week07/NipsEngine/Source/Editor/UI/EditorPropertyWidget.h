
#pragma once
#include "Editor/UI/EditorWidget.h"
#include "Object/Object.h"

class FSelectionManager;
class UActorComponent;
class AActor;

class FEditorPropertyWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;
	void Initialize(UEditorEngine* InEditorEngine) override;

	UActorComponent* GetSelectedComponent() const { return SelectedComponent; }
	bool IsActorSelected() const { return bActorSelected; }
private:
	// 선택 상태 관리
	void UpdateSelectionState(AActor* PrimaryActor);

	// 헤더 영역
	void RenderActorHeaderRegion(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderMultiSelectionHeader(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors, int32 SelectionCount);
	void RenderSingleSelectionHeader(AActor* PrimaryActor);
	void RenderAddComponentPopup(AActor* PrimaryActor);

	// 컴포넌트 트리
	void RenderComponentTree(AActor* Actor);
	void RenderSceneComponentNode(AActor* Actor, class USceneComponent* Comp, UActorComponent*& OutCompToDelete);

	// 디테일 패널
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties();
	void RenderPropertyWidget(struct FPropertyDescriptor& Prop);
	void RenderSceneComponentRefWidget(struct FPropertyDescriptor& Prop, AActor* Owner);
	void RenderInterpControlPoints(class UInterpToMovementComponent* Comp);

	// 유틸리티
	void AttachAndSelectNewComponent(AActor* PrimaryActor, UActorComponent* NewComp);

	// 멤버 변수
	FSelectionManager* SelectionManager  = nullptr;
	UActorComponent* SelectedComponent = nullptr;
	AActor* LastSelectedActor = nullptr;
	bool bActorSelected   = true; // true: Actor details, false: Component details
};
