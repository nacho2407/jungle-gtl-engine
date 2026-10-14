#pragma once

#include "Core/Types/CoreTypes.h"

#include <algorithm>

/**
 * @brief Scene Manager 에디터 그룹 정보
 *
 * @details 런타임 actor 계층과 분리된 에디터 전용 데이터입니다
 */
struct FSceneOutlinerGroup
{
	uint32 GroupId = 0;
	uint32 ParentGroupId = 0;
	FString Name;
	TArray<uint32> ActorUUIDs;
	bool bExpanded = true;
};

/**
 * @brief Scene Manager 에디터 전용 outliner 상태
 *
 * @details 그룹은 actor UUID와 에디터용 부모 그룹 ID만 보관하며, 게임 실행과 actor/component transform에는 영향을 주지 않습니다
 */
struct FSceneOutlinerState
{
	TArray<FSceneOutlinerGroup> Groups;
	uint32 NextGroupId = 1;

	/**
	 * @brief 모든 그룹 상태를 초기화합니다
	 */
	void Clear()
	{
		Groups.clear();
		NextGroupId = 1;
	}

	/**
	 * @brief 지정한 ID의 그룹을 찾습니다
	 *
	 * @param GroupId 찾을 그룹 ID
	 *
	 * @return 찾은 그룹. 없으면 nullptr 반환
	 */
	FSceneOutlinerGroup* FindGroup(uint32 GroupId)
	{
		for (FSceneOutlinerGroup& Group : Groups)
		{
			if (Group.GroupId == GroupId)
			{
				return &Group;
			}
		}
		return nullptr;
	}

	/**
	 * @brief 지정한 ID의 그룹을 찾습니다
	 *
	 * @param GroupId 찾을 그룹 ID
	 *
	 * @return 찾은 그룹. 없으면 nullptr 반환
	 */
	const FSceneOutlinerGroup* FindGroup(uint32 GroupId) const
	{
		for (const FSceneOutlinerGroup& Group : Groups)
		{
			if (Group.GroupId == GroupId)
			{
				return &Group;
			}
		}
		return nullptr;
	}

	/**
	 * @brief 지정한 ID의 그룹이 현재 존재하는지 확인합니다
	 *
	 * @param GroupId 확인할 그룹 ID
	 *
	 * @return 그룹이 존재하면 true
	 */
	bool HasGroup(uint32 GroupId) const
	{
		return FindGroup(GroupId) != nullptr;
	}

	/**
	 * @brief 그룹이 최상위 그룹인지 확인합니다
	 *
	 * @param Group 확인할 그룹
	 *
	 * @return 부모 그룹이 없거나 유효하지 않으면 true
	 */
	bool IsTopLevelGroup(const FSceneOutlinerGroup& Group) const
	{
		return Group.ParentGroupId == 0 || FindGroup(Group.ParentGroupId) == nullptr;
	}

	/**
	 * @brief 지정한 부모 그룹에 속한 하위 그룹 ID를 수집합니다
	 *
	 * @param ParentGroupId 부모 그룹 ID
	 *
	 * @param OutGroupIds 하위 그룹 ID 목록
	 */
	void CollectChildGroupIds(uint32 ParentGroupId, TArray<uint32>& OutGroupIds) const
	{
		for (const FSceneOutlinerGroup& Group : Groups)
		{
			if (Group.ParentGroupId == ParentGroupId)
			{
				OutGroupIds.push_back(Group.GroupId);
			}
		}
	}

	/**
	 * @brief 지정한 그룹에 하위 그룹이 있는지 확인합니다
	 *
	 * @param GroupId 확인할 그룹 ID
	 *
	 * @return 하위 그룹이 있으면 true
	 */
	bool HasChildGroups(uint32 GroupId) const
	{
		for (const FSceneOutlinerGroup& Group : Groups)
		{
			if (Group.ParentGroupId == GroupId)
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief 그룹이 특정 부모 그룹의 하위 계층인지 확인합니다
	 *
	 * @param GroupId 확인할 그룹 ID
	 *
	 * @param PotentialAncestorGroupId 부모 후보 그룹 ID
	 *
	 * @return 하위 계층이면 true
	 */
	bool IsGroupDescendantOf(uint32 GroupId, uint32 PotentialAncestorGroupId) const
	{
		if (GroupId == 0 || PotentialAncestorGroupId == 0 || GroupId == PotentialAncestorGroupId)
		{
			return GroupId != 0 && GroupId == PotentialAncestorGroupId;
		}

		TSet<uint32> VisitedGroupIds;
		const FSceneOutlinerGroup* Group = FindGroup(GroupId);
		while (Group && Group->ParentGroupId != 0)
		{
			if (Group->ParentGroupId == PotentialAncestorGroupId)
			{
				return true;
			}

			if (VisitedGroupIds.find(Group->ParentGroupId) != VisitedGroupIds.end())
			{
				return false;
			}
			VisitedGroupIds.insert(Group->ParentGroupId);
			Group = FindGroup(Group->ParentGroupId);
		}
		return false;
	}

	/**
	 * @brief 선택된 그룹 목록에서 조상 그룹이 이미 포함된 하위 그룹을 제거합니다
	 *
	 * @param GroupIds 정리할 그룹 ID 목록
	 *
	 * @return 최상위 선택 그룹 ID 목록
	 */
	TArray<uint32> MakeRootGroupSelection(const TArray<uint32>& GroupIds) const
	{
		TArray<uint32> Result;
		for (uint32 GroupId : GroupIds)
		{
			if (GroupId == 0 || !FindGroup(GroupId))
			{
				continue;
			}

			bool bHasSelectedAncestor = false;
			for (uint32 OtherGroupId : GroupIds)
			{
				if (OtherGroupId != GroupId && IsGroupDescendantOf(GroupId, OtherGroupId))
				{
					bHasSelectedAncestor = true;
					break;
				}
			}
			if (bHasSelectedAncestor)
			{
				continue;
			}

			if (std::find(Result.begin(), Result.end(), GroupId) == Result.end())
			{
				Result.push_back(GroupId);
			}
		}
		return Result;
	}

	/**
	 * @brief 그룹과 하위 그룹에 포함된 actor UUID를 재귀적으로 수집합니다
	 *
	 * @param GroupId 수집할 그룹 ID
	 *
	 * @param OutActorUUIDs 수집 결과
	 */
	void CollectActorUUIDsRecursive(uint32 GroupId, TArray<uint32>& OutActorUUIDs) const
	{
		TSet<uint32> VisitedGroupIds;
		CollectActorUUIDsRecursive(GroupId, OutActorUUIDs, VisitedGroupIds);
	}

	/**
	 * @brief 여러 그룹의 actor UUID를 중복 없이 재귀 수집합니다
	 *
	 * @param GroupIds 수집할 그룹 ID 목록
	 *
	 * @return actor UUID 목록
	 */
	TArray<uint32> GetActorUUIDsForGroups(const TArray<uint32>& GroupIds) const
	{
		TArray<uint32> Result;
		TSet<uint32> VisitedGroupIds;
		for (uint32 GroupId : GroupIds)
		{
			CollectActorUUIDsRecursive(GroupId, Result, VisitedGroupIds);
		}
		return Result;
	}

	/**
	 * @brief actor가 속한 그룹을 찾습니다
	 *
	 * @param ActorUUID 찾을 actor UUID
	 *
	 * @return actor가 속한 그룹. 없으면 nullptr 반환
	 */
	FSceneOutlinerGroup* FindGroupForActor(uint32 ActorUUID)
	{
		for (FSceneOutlinerGroup& Group : Groups)
		{
			if (std::find(Group.ActorUUIDs.begin(), Group.ActorUUIDs.end(), ActorUUID) != Group.ActorUUIDs.end())
			{
				return &Group;
			}
		}
		return nullptr;
	}

	/**
	 * @brief actor가 어느 그룹에 속해 있는지 확인합니다
	 *
	 * @param ActorUUID 확인할 actor UUID
	 *
	 * @return 그룹에 속해 있으면 true
	 */
	bool IsActorGrouped(uint32 ActorUUID) const
	{
		for (const FSceneOutlinerGroup& Group : Groups)
		{
			if (std::find(Group.ActorUUIDs.begin(), Group.ActorUUIDs.end(), ActorUUID) != Group.ActorUUIDs.end())
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * @brief 모든 그룹에서 actor를 제거합니다
	 *
	 * @param ActorUUID 제거할 actor UUID
	 */
	void RemoveActorFromGroups(uint32 ActorUUID)
	{
		for (FSceneOutlinerGroup& Group : Groups)
		{
			Group.ActorUUIDs.erase(
				std::remove(Group.ActorUUIDs.begin(), Group.ActorUUIDs.end(), ActorUUID),
				Group.ActorUUIDs.end());
		}
		RemoveEmptyGroups();
	}

	/**
	 * @brief actor 목록으로 새 그룹을 생성합니다
	 *
	 * @param Name 생성할 그룹 이름
	 *
	 * @param ActorUUIDs 그룹에 넣을 actor UUID 목록
	 *
	 * @param ChildGroupIds 새 그룹의 하위 그룹으로 옮길 그룹 ID 목록
	 *
	 * @return 생성된 그룹 ID. 유효한 actor와 하위 그룹이 없으면 0 반환
	 */
	uint32 CreateGroup(
		const FString& Name,
		const TArray<uint32>& ActorUUIDs,
		const TArray<uint32>& ChildGroupIds = TArray<uint32>())
	{
		TArray<uint32> UniqueActorUUIDs;
		for (uint32 ActorUUID : ActorUUIDs)
		{
			if (ActorUUID == 0)
			{
				continue;
			}

			RemoveActorFromGroups(ActorUUID);
			if (std::find(UniqueActorUUIDs.begin(), UniqueActorUUIDs.end(), ActorUUID) == UniqueActorUUIDs.end())
			{
				UniqueActorUUIDs.push_back(ActorUUID);
			}
		}

		TArray<uint32> UniqueChildGroupIds = MakeRootGroupSelection(ChildGroupIds);
		if (UniqueActorUUIDs.empty() && UniqueChildGroupIds.empty())
		{
			return 0;
		}

		FSceneOutlinerGroup Group;
		Group.GroupId = NextGroupId++;
		Group.ParentGroupId = 0;
		Group.Name = Name.empty() ? MakeDefaultGroupName(Group.GroupId) : Name;
		Group.ActorUUIDs = std::move(UniqueActorUUIDs);
		Group.bExpanded = true;
		Groups.push_back(std::move(Group));

		for (uint32 ChildGroupId : UniqueChildGroupIds)
		{
			if (FSceneOutlinerGroup* ChildGroup = FindGroup(ChildGroupId))
			{
				ChildGroup->ParentGroupId = Groups.back().GroupId;
			}
		}

		return Groups.back().GroupId;
	}

	/**
	 * @brief actor 없이 새 그룹을 생성합니다
	 *
	 * @param Name 생성할 그룹 이름
	 *
	 * @return 생성된 그룹 ID
	 */
	uint32 CreateEmptyGroup(const FString& Name)
	{
		FSceneOutlinerGroup Group;
		Group.GroupId = NextGroupId++;
		Group.ParentGroupId = 0;
		Group.Name = Name.empty() ? MakeDefaultGroupName(Group.GroupId) : Name;
		Group.bExpanded = true;
		Groups.push_back(std::move(Group));
		return Groups.back().GroupId;
	}

	/**
	 * @brief 그룹을 삭제하고 하위 actor는 유지합니다
	 *
	 * @param GroupId 삭제할 그룹 ID
	 */
	void RemoveGroup(uint32 GroupId)
	{
		FSceneOutlinerGroup* GroupToRemove = FindGroup(GroupId);
		if (!GroupToRemove)
		{
			return;
		}

		const uint32 ParentGroupId = (GroupToRemove->ParentGroupId != GroupId && FindGroup(GroupToRemove->ParentGroupId))
			? GroupToRemove->ParentGroupId
			: 0;
		TArray<uint32> ActorUUIDsToPromote = GroupToRemove->ActorUUIDs;
		if (FSceneOutlinerGroup* ParentGroup = FindGroup(ParentGroupId))
		{
			for (uint32 ActorUUID : ActorUUIDsToPromote)
			{
				if (ActorUUID != 0
					&& std::find(ParentGroup->ActorUUIDs.begin(), ParentGroup->ActorUUIDs.end(), ActorUUID) == ParentGroup->ActorUUIDs.end())
				{
					ParentGroup->ActorUUIDs.push_back(ActorUUID);
				}
			}
		}

		for (FSceneOutlinerGroup& Group : Groups)
		{
			if (Group.ParentGroupId == GroupId)
			{
				Group.ParentGroupId = ParentGroupId;
			}
		}

		Groups.erase(
			std::remove_if(
				Groups.begin(),
				Groups.end(),
				[GroupId](const FSceneOutlinerGroup& Group)
				{
					return Group.GroupId == GroupId;
				}),
			Groups.end());
		RemoveEmptyGroups();
	}

	/**
	 * @brief 빈 그룹을 모두 제거합니다
	 */
	void RemoveEmptyGroups()
	{
		bool bRemovedGroup = true;
		while (bRemovedGroup)
		{
			bRemovedGroup = false;
			Groups.erase(
				std::remove_if(
					Groups.begin(),
					Groups.end(),
					[this, &bRemovedGroup](const FSceneOutlinerGroup& Group)
					{
						const bool bShouldRemove = Group.ActorUUIDs.empty() && !HasChildGroups(Group.GroupId);
						bRemovedGroup = bRemovedGroup || bShouldRemove;
						return bShouldRemove;
					}),
				Groups.end());
		}
	}

private:
	/**
	 * @brief 그룹과 하위 그룹에 포함된 actor UUID를 재귀적으로 수집합니다
	 *
	 * @param GroupId 수집할 그룹 ID
	 *
	 * @param OutActorUUIDs 수집 결과
	 *
	 * @param VisitedGroupIds 순환 방지용 방문 그룹 ID
	 */
	void CollectActorUUIDsRecursive(uint32 GroupId, TArray<uint32>& OutActorUUIDs, TSet<uint32>& VisitedGroupIds) const
	{
		if (GroupId == 0 || VisitedGroupIds.find(GroupId) != VisitedGroupIds.end())
		{
			return;
		}
		VisitedGroupIds.insert(GroupId);

		const FSceneOutlinerGroup* Group = FindGroup(GroupId);
		if (!Group)
		{
			return;
		}

		for (uint32 ActorUUID : Group->ActorUUIDs)
		{
			if (ActorUUID != 0
				&& std::find(OutActorUUIDs.begin(), OutActorUUIDs.end(), ActorUUID) == OutActorUUIDs.end())
			{
				OutActorUUIDs.push_back(ActorUUID);
			}
		}

		for (const FSceneOutlinerGroup& ChildGroup : Groups)
		{
			if (ChildGroup.ParentGroupId == GroupId)
			{
				CollectActorUUIDsRecursive(ChildGroup.GroupId, OutActorUUIDs, VisitedGroupIds);
			}
		}
	}

public:
	/**
	 * @brief 기본 그룹 이름을 생성합니다
	 *
	 * @param GroupId 이름에 사용할 그룹 ID
	 *
	 * @return 기본 그룹 이름
	 */
	static FString MakeDefaultGroupName(uint32 GroupId)
	{
		return "Group " + std::to_string(GroupId);
	}
};
