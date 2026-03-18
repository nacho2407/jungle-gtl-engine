#include "FUObjectArray.h"

void FUObjectClusterContainer::DissolveCluster(FUObjectCluster& Cluster)
{
	FUObjectItem* RootObjectItem = GUObjectArray.IndexToObject(Cluster.RootIndex);

	const int32 OldClusterIndex = RootObjectItem->GetClusterIndex();
	for (int32 ClusterObjectIndex : Cluster.Objects)
	{
		FUObjectItem* ClusterObjectItem = GUObjectArray.IndexToObject(ClusterObjectIndex);
		ClusterObjectItem->SetOwnerIndex(0);
	}

	FreeCluster(OldClusterIndex);
}

FUObjectClusterContainer::FUObjectClusterContainer()
	: NumAllocatedClusters(0)
	, bClustersNeedDissolving(false)
{

}

void FUObjectClusterContainer::FreeCluster(int32 InClusterIndex)
{
	FUObjectCluster& Cluster = Clusters[InClusterIndex];
	check(Cluster.RootIndex != -1);
	FUObjectItem* RootItem = GUObjectArray.IndexToObject(Cluster.RootIndex);
	check(RootItem->GetClusterIndex() == InClusterIndex);
	RootItem->SetOwnerIndex(0);

	Cluster.RootIndex = -1;
	Cluster.Objects.clear();
	Cluster.MutableObjects.clear();
	Cluster.ReferencedClusters.clear();
	Cluster.ReferencedByClusters.clear();

	Cluster.bNeedsDissolving = false;
	FreeClusterIndices.push_back(InClusterIndex);
	NumAllocatedClusters--;
	check(NumAllocatedClusters >= 0);
}

FUObjectCluster* FUObjectClusterContainer::GetObjectCluster(UObject* ClusterRootOrObjectFromCluster)
{
	check(ClusterRootOrObjectFromCluster);

	const int32 OuterIndex = GUObjectArray.ObjectToIndex(ClusterRootOrObjectFromCluster);
	FUObjectItem* OuterItem = GUObjectArray.IndexToObject(OuterIndex);
	int32 ClusterRootIndex = 0;
	if (OuterItem->bIsClusterRoot)
	{
		ClusterRootIndex = OuterIndex;
	}
	else
	{
		ClusterRootIndex = OuterItem->GetOwnerIndex();
	}
	FUObjectCluster* Cluster = nullptr;
	if (ClusterRootIndex != 0)
	{
		const int32 ClusterIndex = ClusterRootIndex > 0 ? GUObjectArray.IndexToObject(ClusterRootIndex)->GetClusterIndex() : OuterItem->GetClusterIndex();
		Cluster = &GUObjectClusters[ClusterIndex];
	}
	return Cluster;
}

void FUObjectClusterContainer::DissolveCluster(UObject* ClusterRootOrObjectFromCluster)
{
	FUObjectCluster* Cluster = GetObjectCluster(ClusterRootOrObjectFromCluster);
	if (Cluster)
	{
		DissolveCluster(*Cluster);
	}
}
int32 FUObjectClusterContainer::AllocateCluster(int32 InRootObjectIndex)
{
	int32 ClusterIndex = -1;
	if (FreeClusterIndices.size())
	{
		ClusterIndex = FreeClusterIndices.back();
		FreeClusterIndices.pop_back();
	}
	else
	{
		FUObjectCluster newCluster = FUObjectCluster();
		ClusterIndex = static_cast<int32>(Clusters.size());
		Clusters.push_back(FUObjectCluster());
	}
	FUObjectCluster& NewCluster = Clusters[ClusterIndex];
	check(NewCluster.RootIndex == -1);
	NewCluster.RootIndex = InRootObjectIndex;
	NumAllocatedClusters++;
	return ClusterIndex;
}
