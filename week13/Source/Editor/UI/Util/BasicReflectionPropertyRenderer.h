#pragma once

// 순수 Reflection 기반 기본 Property Renderer 규칙
// 1. 최상위 UPROPERTY만 수집한다.
// 2. 렌더링 대상은 에디터 노출 플래그인 PF_Edit가 있는 property로 제한한다.
// 3. Category grouping은 최상위 property 기준으로만 한다.
// 4. table column은 0, 1 두 개만 사용한다.
// 5. column 0에는 property 이름과 Struct / Array open-close UI를 둔다.
// 6. column 1에는 값 widget만 둔다.
// 7. Struct / Array만 렌더링 중 같은 규칙으로 재귀로 펼친다.
// 8. ObjectRef는 내부 property로 들어가지 않고 참조 슬롯으로만 그린다.
// 9. Mesh / Material / PhysicsAsset picker, import, script edit 같은 특수 UI는 넣지 않는다.
// 10. 멀티 select에서는 선언 owner class + property path + EPropertyType이 같은 property만 렌더링한다.
// 11. Header category는 항상 최상단에 고정 표시하고 일반 category에서 중복 렌더링하지 않는다.
// 12. Transform category는 Header 다음 최상단에 Location, Rotation, Scale 순서로 고정 표시하고 일반 category에서 중복 렌더링하지 않는다.

// 13. Array 구조 편집은 add/remove만 지원하고 reorder, drag/drop, custom element action은 넣지 않는다.

#include "Core/Types/CoreTypes.h"
#include "Editor/Selection/SelectionManager.h"

struct FProperty;
struct FPropertyValue;

class FBasicReflectionPropertyRenderer
{
public:
	bool Render(const TArray<FSelectionDetailTarget>& Targets, const char* TableId = "##BasicReflectionProperties");

private:
	struct FCategoryGroup;

	void CollectTopLevelCategories(
		const FSelectionDetailTarget& PrimaryTarget,
		const TArray<FSelectionDetailTarget>& Targets,
		TArray<FCategoryGroup>& OutCategories) const;
	bool BuildCompatibleValues(
		const FProperty* PrimaryProperty,
		const FSelectionDetailTarget& PrimaryTarget,
		const TArray<FSelectionDetailTarget>& Targets,
		TArray<FPropertyValue>& OutValues) const;
	bool RenderSpecialCategories(
		const FSelectionDetailTarget& PrimaryTarget,
		const TArray<FSelectionDetailTarget>& Targets,
		const char* TableId,
		bool& bOutRenderedAny);
	bool RenderHeaderCategory(
		const FSelectionDetailTarget& PrimaryTarget,
		const TArray<FSelectionDetailTarget>& Targets,
		const char* TableId,
		bool& bOutRenderedAny);
	bool RenderTransformCategory(
		const FSelectionDetailTarget& PrimaryTarget,
		const TArray<FSelectionDetailTarget>& Targets,
		const char* TableId,
		bool& bOutRenderedAny);
	bool RenderCategory(
		const FCategoryGroup& Category,
		const FSelectionDetailTarget& PrimaryTarget,
		const TArray<FSelectionDetailTarget>& Targets,
		const char* TableId);
	bool RenderPropertyRow(
		FPropertyValue& PrimaryValue,
		const TArray<FPropertyValue>& CompatibleValues,
		const FString& PropertyPath,
		int32 Depth,
		const char* LabelOverride = nullptr,
		bool bShowRemoveButton = false,
		bool bCanRemove = false,
		bool* bOutRemoveClicked = nullptr);
	bool RenderStructChildren(
		FPropertyValue& PrimaryValue,
		const TArray<FPropertyValue>& CompatibleValues,
		const FString& PropertyPath,
		int32 Depth);
	bool RenderArrayChildren(
		FPropertyValue& PrimaryValue,
		const TArray<FPropertyValue>& CompatibleValues,
		const FString& PropertyPath,
		int32 Depth);
	bool RenderLeafValue(FPropertyValue& Value, bool bReadOnly);

	void PropagateValueChange(
		const FPropertyValue& SourceValue,
		const TArray<FPropertyValue>& CompatibleValues,
		const FString& PropertyPath) const;
};
