#pragma once

#include "Editor/UI/EditorWidget.h"
#include "Editor/UI/Panel/EditorPropertyRenderer.h"
#include "Editor/Undo/EditorUndoCommand.h"
#include "Object/Object.h"

class UActorComponent;
class AActor;

/**
 * @brief Details 속성 undo 진행 상태
 */
struct FDetailsPropertyUndoTransaction
{
	bool bActive = false;
	TArray<uint32> TargetObjectUUIDs;
	TArray<FEditorObjectPropertySnapshot> BeforeSnapshots;
	FEditorSelectionSnapshot Selection;
	FString DebugName;
};

class FEditorPropertyWidget : public FEditorWidget
{
public:
	virtual void Render(float DeltaTime) override;
	void SetShowEditorOnlyComponents(bool bEnable) { bShowEditorOnlyComponents = bEnable; }
	bool IsShowingEditorOnlyComponents() const { return bShowEditorOnlyComponents; }

	/**
	 * @brief 진행 중인 Details 속성 undo 트랜잭션을 외부 요청으로 확정합니다.
	 *
	 * @details 전역 undo/redo 단축키가 실행되기 직전에 현재 Details 편집 결과를 undo stack에 먼저 반영하기 위한 진입점입니다.
	 */
	void FlushPendingDetailsUndoTransaction();

	/**
	 * @brief 선택된 non-root component를 undo 기록과 함께 삭제합니다.
	 *
	 * @return component 삭제 실행 여부
	 */
	bool DeleteSelectedComponentWithUndo();

private:
	void RenameActor(AActor* PrimaryActor);
	void RenameComponent(AActor* OwnerActor, UActorComponent* Component);
	void RenderComponentTree(AActor* Actor);
	void RenderSceneComponentNode(class USceneComponent* Comp);
	void RenderDetails(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);
	void RenderComponentProperties(AActor* Actor, const TArray<AActor*>& SelectedActors);
	void RenderActorProperties(AActor* PrimaryActor, const TArray<AActor*>& SelectedActors);

	void AddComponentToActor(AActor* Actor, UClass* ComponentClass);

	/**
	 * @brief Details 속성 변경을 undo stack에 기록하거나 진행 중인 트랜잭션으로 보관합니다.
	 *
	 * @param BeforeSnapshots 속성 변경 전 객체 스냅샷 목록
	 *
	 * @param TargetObjects 속성 변경 대상 객체 목록
	 *
	 * @param DebugName 디버그 표시용 명령 이름
	 */
	void RecordDetailsPropertyUndoChange(
		const TArray<FEditorObjectPropertySnapshot>& BeforeSnapshots,
		const TArray<UObject*>& TargetObjects,
		const FString& DebugName);

	/**
	 * @brief 진행 중인 Details 속성 undo 트랜잭션을 즉시 확정합니다.
	 */
	void CommitActiveDetailsPropertyUndo();

	/**
	 * @brief ImGui 편집이 끝난 경우 진행 중인 Details 속성 undo 트랜잭션을 확정합니다.
	 */
	void CommitActiveDetailsPropertyUndoIfIdle();

	/**
	 * @brief actor rename buffer를 실제 actor 이름과 동기화합니다.
	 *
	 * @param Actor 동기화 대상 actor
	 *
	 * @param bRenameInputActive rename input 활성 상태
	 */
	void SyncActorRenameBufferIfNeeded(AActor* Actor, bool bRenameInputActive);

	/**
	 * @brief component rename buffer를 실제 component 이름과 동기화합니다.
	 *
	 * @param Component 동기화 대상 component
	 *
	 * @param bRenameInputActive rename input 활성 상태
	 */
	void SyncComponentRenameBufferIfNeeded(UActorComponent* Component, bool bRenameInputActive);

	/**
	 * @brief Details component 구조 변경을 undo stack에 기록합니다.
	 *
	 * @param Actor 구조가 변경된 actor
	 *
	 * @param BeforeActorJSON 구조 변경 전 actor/component 스냅샷
	 *
	 * @param SelectionBefore 구조 변경 전 선택 상태
	 *
	 * @param DebugName 디버그 표시용 명령 이름
	 */
	void RecordActorStructureUndoChange(
		AActor* Actor,
		json::JSON BeforeActorJSON,
		const FEditorSelectionSnapshot& SelectionBefore,
		const FString& DebugName);

	/**
	 * @brief 현재 Details 선택 component가 actor component 목록에 남아 있는지 확인합니다.
	 *
	 * @param Actor 소유 여부를 확인할 actor
	 *
	 * @return 선택 component가 유효하고 actor가 소유 중이면 true
	 */
	bool IsSelectedComponentOwnedByActor(AActor* Actor) const;

	/**
	 * @brief undo/redo 이후 Details 선택 component 포인터를 현재 selection 상태와 동기화합니다.
	 *
	 * @param Actor 동기화 기준 actor
	 */
	void SyncSelectedComponentAfterStructureChange(AActor* Actor);

	UActorComponent* SelectedComponent = nullptr;
	AActor* LastSelectedActor = nullptr;
	bool bActorSelected = true; // true: Actor details, false: Component details
	bool bShowEditorOnlyComponents = false;

	char RenameBuffer[256] = {};
	FString RenameWarning;
	FName LastObservedActorName;
	char ComponentRenameBuffer[256] = {};
	FString ComponentRenameWarning;
	UActorComponent* LastRenameComponent = nullptr;
	FName LastObservedComponentName;

	float PendingDetailsScrollY = -1.0f;

	FEditorPropertyRenderer PropertyRenderer;
	FDetailsPropertyUndoTransaction ActivePropertyUndoTransaction;
};
