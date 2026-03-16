#pragma once

#include "CoreDefine.h"
#include "Private/FMemory.h"
#include "UObject.h"
#include "UObjectBaseUtility.h"

class UObject;

class FUObjectItem
{
private:
	UObjectBase* Object;

public:
	UObjectBase* GetObject();

	int32 SerialNumber;
	int32 ClusterRootIndex;
	int32 ClusterIndex;
	int32 ObjectSize;

	inline void SetObject(UObjectBase* InObject)
	{
		Object = InObject;
	}
	void SetOwnerIndex(int32 OwnerIndex)
	{
		ClusterRootIndex = OwnerIndex;
	}
	int32 GetOwnerIndex() const
	{
		return ClusterRootIndex;
	}
	int32 GetSerialNumber() const
	{
		return SerialNumber;
	}
	int32 GetClusterIndex()
	{
		return ClusterIndex;
	}
	void SetClusterIndex(int32 ClusterIndex)
	{
		ClusterRootIndex = -ClusterIndex - 1;
	}
};

class FChunckedFixedObjectArray
{
private:
	enum 
	{ 
		NumElementsPerChunk = 1024 * 64, 
	};

	FUObjectItem** Objects;
	FUObjectItem* PreAllocatedObjects;
	int32 MaxElements;
	int32 NumElements;
	int32 MaxChunks;
	int32 NumChunks;

public:
	FChunckedFixedObjectArray();
	virtual ~FChunckedFixedObjectArray();

	void ExpandChunksToIndex(int32 Index);

	void PreAllocate(int32 InMaxElements);

	int32 Num() const;
	int32 Capacity() const;
	int32 IsValidIndex(int32 Index);

	FUObjectItem* GetObjectPtr(int32 Index);
	FUObjectItem const* GetObjectPtr(int32 Index) const;

	int32 AddRange(int32 NumToAdd);
	int32 AddSingle();

	FUObjectItem const& operator[](int32 Index) const
	{
		FUObjectItem const* ItemPtr = GetObjectPtr(Index);
		check(ItemPtr);
		return *ItemPtr;
	}
	FUObjectItem& operator[](int32 Index)
	{
		FUObjectItem* ItemPtr = GetObjectPtr(Index);
		check(ItemPtr != nullptr);
		return *ItemPtr;
	}
};

class FUObjectArray
{
private:
	typedef FChunckedFixedObjectArray TUObjectArray;

	TUObjectArray Objects;
	TArray<int32> ObjAvailableList;
	int32 ObjAvailableListEstimateCount;
public:
	uint32 ElementalCount;
	FUObjectArray() : ObjAvailableListEstimateCount(0) 
	{
		Objects.PreAllocate(1024);
	};
	void AllocatdUObjectIndex(UObjectBaseUtility* Object, int32 SerialNumber);
	void FreeUObjectIndox(UObject* Object);
	int32 ObjectToIndex(UObjectBaseUtility* Object) const
	{
		return Object->InternalIndex;
	}
	FUObjectItem* IndexToObject(int32 Index)
	{
		check(Index >= 0);
		//if (Index < Objects.Num())
		//{
		//	return const_cast<FUObjectItem*>(&Objects[Index]);
		//}

		if (Index <= Objects.Num())
		{
			return const_cast<FUObjectItem*>(&Objects[Index]);
		}

		return nullptr;
	}
	FUObjectItem* ObjectToObjectItem(const UObjectBaseUtility* Object)
	{
		FUObjectItem* ObjectItem = IndexToObject(Object->InternalIndex);
		return ObjectItem;
	}

	int32 AllocateSerialNumber(int32 Index);
	int32 GetSerialNumber(int32 Index)
	{
		FUObjectItem* ObjectItem = IndexToObject(Index);
		return ObjectItem->GetSerialNumber();
	}

	int32 GetObjectArrayNum() const
	{
		return Objects.Num();
	}

	int32 GetAvailableIndex() const
	{
		return ObjAvailableList.size() != 0 ? ObjAvailableList.back() : Objects.Num();
	}

	int32 Num() const
	{
		return Objects.Num();
	}
};

struct FUObjectCluster
{
	FUObjectCluster()
		: RootIndex(-1)
		, bNeedsDissolving(false)
	{
	}

	/** Root object index */
	int32 RootIndex;
	/** Objects that belong to this cluster */
	TArray<int32> Objects;
	/** Other clusters referenced by this cluster */
	TArray<int32> ReferencedClusters;
	/** Objects that could not be added to the cluster but still need to be referenced by it */
	TArray<int32> MutableObjects;
	/** List of clusters that direcly reference this cluster. Used when dissolving a cluster. */
	TArray<int32> ReferencedByClusters;
#if WITH_VERSE_VM || defined(__INTELLISENSE__)
	/** All verse cells are considered mutable.  They will just be added directly to verse gc when the cluster is marked */
	//TArray<Verse::VCell*> MutableCells;
#endif

	/** Cluster needs dissolving, probably due to PendingKill reference */
	bool bNeedsDissolving;
};

class FUObjectClusterContainer
{
	/** List of all clusters */
	TArray<FUObjectCluster> Clusters;
	/** List of available cluster indices */
	TArray<int32> FreeClusterIndices;
	/** Number of allocated clusters */
	int32 NumAllocatedClusters;
	/** Clusters need dissolving, probably due to PendingKill reference */
	bool bClustersNeedDissolving;

	/** Dissolves a cluster */
	void DissolveCluster(FUObjectCluster& Cluster);

public:

	FUObjectClusterContainer();

	 FUObjectCluster& operator[](int32 Index)
	{
		check(Index > 0 || Index <= Clusters.size())
		return Clusters[Index];
	}

	/** Returns an index to a new cluster */
	int32 AllocateCluster(int32 InRootObjectIndex);

	/** Frees the cluster at the specified index */
	void FreeCluster(int32 InClusterIndex);

	/**
	* Gets the cluster the specified object is a root of or belongs to.
	* @Param ClusterRootOrObjectFromCluster Root cluster object or object that belongs to a cluster
	*/
	FUObjectCluster* GetObjectCluster(UObjectBaseUtility* ClusterRootOrObjectFromCluster);


	/**
	 * Dissolves a cluster and all clusters that reference it
	 * @Param ClusterRootOrObjectFromCluster Root cluster object or object that belongs to a cluster
	 */
	void DissolveCluster(UObjectBaseUtility* ClusterRootOrObjectFromCluster);

	/** Gets the clusters array (for internal use only!) */
	TArray<FUObjectCluster>& GetClustersUnsafe()
	{
		return Clusters;
	}

	/** Returns the number of currently allocated clusters */
	int32 GetNumAllocatedClusters() const
	{
		return NumAllocatedClusters;
	}

	/** Lets the FUObjectClusterContainer know some clusters need dissolving */
	void SetClustersNeedDissolving()
	{
		bClustersNeedDissolving = true;
	}

	/** Checks if any clusters need dissolving */
	bool ClustersNeedDissolving() const
	{
		return bClustersNeedDissolving;
	}
};


inline FUObjectArray GUObjectArray;
inline FUObjectClusterContainer GUObjectClusters;