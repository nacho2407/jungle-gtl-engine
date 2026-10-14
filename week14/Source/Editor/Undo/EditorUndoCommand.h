#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Transform.h"
#include "Object/FName.h"
#include "SimpleJSON/json.hpp"

#include <memory>

class AActor;
class FSelectionManager;
class UObject;
class UEditorEngine;

/**
 * @brief 에디터 선택 상태 스냅샷
 */
struct FEditorSelectionSnapshot
{
	TArray<uint32> ActorUUIDs;
	TArray<uint32> GroupIds;
	uint32 ComponentUUID = 0;
};

/**
 * @brief 씬 컴포넌트 transform 스냅샷
 */
struct FEditorComponentTransformSnapshot
{
	uint32 ComponentUUID = 0;
	FTransform RelativeTransform;
};

/**
 * @brief UObject 속성 상태 스냅샷
 */
struct FEditorObjectPropertySnapshot
{
	uint32 ObjectUUID = 0;
	json::JSON ObjectJSON;
};

/**
 * @brief 에디터 undo/redo 명령 인터페이스
 */
class IEditorUndoCommand
{
public:
	virtual ~IEditorUndoCommand() = default;

	/**
	 * @brief 명령 실행 전 상태로 되돌립니다.
	 *
	 * @param EditorEngine 명령을 적용할 에디터 엔진
	 */
	virtual void Undo(UEditorEngine* EditorEngine) = 0;

	/**
	 * @brief Undo로 되돌린 명령을 다시 적용합니다.
	 *
	 * @param EditorEngine 명령을 적용할 에디터 엔진
	 */
	virtual void Redo(UEditorEngine* EditorEngine) = 0;

	/**
	 * @brief 디버그 표시용 명령 이름을 반환합니다.
	 *
	 * @return 디버그 표시용 명령 이름
	 */
	virtual FString GetDebugName() const = 0;
};

/**
 * @brief 현재 선택 상태를 UUID 기반 스냅샷으로 캡처합니다.
 *
 * @param SelectionManager 선택 상태를 읽을 selection manager
 *
 * @return UUID 기반 선택 상태 스냅샷
 */
FEditorSelectionSnapshot CaptureEditorSelection(const FSelectionManager* SelectionManager);

/**
 * @brief 현재 gizmo 조작 대상의 transform 스냅샷을 캡처합니다.
 *
 * @param SelectionManager 선택 상태를 읽을 selection manager
 *
 * @return 선택된 component와 actor root component transform 스냅샷 목록
 */
TArray<FEditorComponentTransformSnapshot> CaptureSelectedComponentTransforms(const FSelectionManager* SelectionManager);

/**
 * @brief UObject 속성 상태를 UUID 기반 스냅샷으로 캡처합니다.
 *
 * @param Objects 스냅샷을 생성할 객체 목록
 *
 * @return 객체 UUID와 직렬화된 속성 JSON 목록
 */
TArray<FEditorObjectPropertySnapshot> CaptureObjectPropertySnapshots(const TArray<UObject*>& Objects);

/**
 * @brief 두 transform 스냅샷 목록이 실질적으로 같은지 확인합니다.
 *
 * @param A 비교 대상 스냅샷 목록
 *
 * @param B 비교 대상 스냅샷 목록
 *
 * @return 두 목록의 대상과 transform이 같으면 true
 */
bool AreComponentTransformSnapshotsEqual(
	const TArray<FEditorComponentTransformSnapshot>& A,
	const TArray<FEditorComponentTransformSnapshot>& B);

/**
 * @brief actor 생성 계열 undo command를 생성합니다.
 *
 * @param Actors 이미 생성된 actor 목록
 *
 * @param SelectionBefore 명령 실행 전 선택 상태
 *
 * @param SelectionAfter 명령 실행 후 선택 상태
 *
 * @param DebugName 디버그 표시용 명령 이름
 *
 * @return 생성된 undo command. 기록할 actor가 없으면 nullptr
 */
std::unique_ptr<IEditorUndoCommand> MakeActorCreateUndoCommand(
	const TArray<AActor*>& Actors,
	const FEditorSelectionSnapshot& SelectionBefore,
	const FEditorSelectionSnapshot& SelectionAfter,
	const FString& DebugName);

/**
 * @brief actor 삭제 undo command를 생성합니다.
 *
 * @param Actors 삭제 직전 actor 목록
 *
 * @param SelectionBefore 명령 실행 전 선택 상태
 *
 * @param SelectionAfter 명령 실행 후 선택 상태
 *
 * @param DebugName 디버그 표시용 명령 이름
 *
 * @return 생성된 undo command. 기록할 actor가 없으면 nullptr
 */
std::unique_ptr<IEditorUndoCommand> MakeActorDeleteUndoCommand(
	const TArray<AActor*>& Actors,
	const FEditorSelectionSnapshot& SelectionBefore,
	const FEditorSelectionSnapshot& SelectionAfter,
	const FString& DebugName);

/**
 * @brief actor 내부 component 구조 변경 undo command를 생성합니다.
 *
 * @param Actor component 구조가 변경된 actor
 *
 * @param BeforeActorJSON 명령 실행 전 actor/component 구조 스냅샷
 *
 * @param AfterActorJSON 명령 실행 후 actor/component 구조 스냅샷
 *
 * @param SelectionBefore 명령 실행 전 선택 상태
 *
 * @param SelectionAfter 명령 실행 후 선택 상태
 *
 * @param DebugName 디버그 표시용 명령 이름
 *
 * @return 생성된 undo command. 구조 변경이 없으면 nullptr
 */
std::unique_ptr<IEditorUndoCommand> MakeActorStructureUndoCommand(
	AActor* Actor,
	json::JSON BeforeActorJSON,
	json::JSON AfterActorJSON,
	const FEditorSelectionSnapshot& SelectionBefore,
	const FEditorSelectionSnapshot& SelectionAfter,
	const FString& DebugName);

/**
 * @brief transform 변경 undo command를 생성합니다.
 *
 * @param Before 명령 실행 전 transform 스냅샷 목록
 *
 * @param After 명령 실행 후 transform 스냅샷 목록
 *
 * @param Selection 명령 전후에 유지할 선택 상태
 *
 * @param DebugName 디버그 표시용 명령 이름
 *
 * @return 생성된 undo command. transform 변경이 없으면 nullptr
 */
std::unique_ptr<IEditorUndoCommand> MakeTransformUndoCommand(
	const TArray<FEditorComponentTransformSnapshot>& Before,
	const TArray<FEditorComponentTransformSnapshot>& After,
	const FEditorSelectionSnapshot& Selection,
	const FString& DebugName);

/**
 * @brief 객체 속성 변경 undo 명령을 생성합니다.
 *
 * @param Before 명령 실행 전 객체 속성 스냅샷 목록
 *
 * @param After 명령 실행 후 객체 속성 스냅샷 목록
 *
 * @param Selection 명령 전후에 유지할 선택 상태
 *
 * @param DebugName 디버그 표시용 명령 이름
 *
 * @return 생성된 undo command. property 변경이 없으면 nullptr
 */
std::unique_ptr<IEditorUndoCommand> MakeObjectPropertyUndoCommand(
	const TArray<FEditorObjectPropertySnapshot>& Before,
	const TArray<FEditorObjectPropertySnapshot>& After,
	const FEditorSelectionSnapshot& Selection,
	const FString& DebugName);

/**
 * @brief 객체 이름 변경 undo 명령을 생성합니다.
 *
 * @param Object 이름이 변경된 객체
 *
 * @param BeforeName 명령 실행 전 객체 이름
 *
 * @param AfterName 명령 실행 후 객체 이름
 *
 * @param Selection 명령 전후에 유지할 선택 상태
 *
 * @param DebugName 디버그 표시용 명령 이름
 *
 * @return 생성된 undo command. 이름 변경이 없으면 nullptr
 */
std::unique_ptr<IEditorUndoCommand> MakeObjectRenameUndoCommand(
	UObject* Object,
	const FName& BeforeName,
	const FName& AfterName,
	const FEditorSelectionSnapshot& Selection,
	const FString& DebugName);
