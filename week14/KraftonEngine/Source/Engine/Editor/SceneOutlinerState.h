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
	FString Name;
	TArray<uint32> ActorUUIDs;
	bool bExpanded = true;
};

/**
 * @brief Scene Manager 에디터 전용 outliner 상태
 *
 * @details 그룹은 actor UUID만 보관하며, 게임 실행과 actor/component transform에는 영향을 주지 않습니다
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
	 * @return 생성된 그룹 ID. 유효한 actor가 없으면 0 반환
	 */
	uint32 CreateGroup(const FString& Name, const TArray<uint32>& ActorUUIDs)
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

		if (UniqueActorUUIDs.empty())
		{
			return 0;
		}

		FSceneOutlinerGroup Group;
		Group.GroupId = NextGroupId++;
		Group.Name = Name.empty() ? MakeDefaultGroupName(Group.GroupId) : Name;
		Group.ActorUUIDs = std::move(UniqueActorUUIDs);
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
		Groups.erase(
			std::remove_if(
				Groups.begin(),
				Groups.end(),
				[GroupId](const FSceneOutlinerGroup& Group)
				{
					return Group.GroupId == GroupId;
				}),
			Groups.end());
	}

	/**
	 * @brief 빈 그룹을 모두 제거합니다
	 */
	void RemoveEmptyGroups()
	{
		Groups.erase(
			std::remove_if(
				Groups.begin(),
				Groups.end(),
				[](const FSceneOutlinerGroup& Group)
				{
					return Group.ActorUUIDs.empty();
				}),
			Groups.end());
	}

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
