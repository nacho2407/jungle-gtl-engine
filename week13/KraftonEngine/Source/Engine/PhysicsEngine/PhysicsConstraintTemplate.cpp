//#include "PhysicsConstraintTemplate.h"
//
//bool UPhysicsConstraintTemplate::ContainsConstraintProfile(FName ProfileName) const
//{
//	for (const FPhysicsConstraintProfileHandle& Handle : ProfileHandles)
//	{
//		if (Handle.ProfileName == ProfileName)
//		{
//			return true;
//		}
//	}
//
//	return false;
//}
//
//void UPhysicsConstraintTemplate::AddConstraintProfile(FName ProfileName)
//{
//	if (ContainsConstraintProfile(ProfileName))
//	{
//		return;
//	}
//
//	FPhysicsConstraintProfileHandle NewHandle;
//	NewHandle.ProfileName = ProfileName;
//	NewHandle.ProfileProperties = DefaultInstance.ProfileInstance;
//	ProfileHandles.push_back(NewHandle);
//}
//
//void UPhysicsConstraintTemplate::RemoveConstraintProfile(FName ProfileName)
//{
//	for (auto It = ProfileHandles.begin(); It != ProfileHandles.end();)
//	{
//		if (It->ProfileName == ProfileName)
//		{
//			It = ProfileHandles.erase(It);
//		}
//		else
//		{
//			++It;
//		}
//	}
//}
//
//void UPhysicsConstraintTemplate::ApplyConstraintProfile(
//	FName ProfileName,
//	FConstraintInstance& ConstraintInstance,
//	bool bDefaultIfNotFound) const
//{
//	for (const FPhysicsConstraintProfileHandle& Handle : ProfileHandles)
//	{
//		if (Handle.ProfileName == ProfileName)
//		{
//			ConstraintInstance.CopyProfilePropertiesFrom(Handle.ProfileProperties);
//			return;
//		}
//	}
//
//	if (bDefaultIfNotFound)
//	{
//		ConstraintInstance.CopyProfilePropertiesFrom(DefaultInstance.ProfileInstance);
//	}
//}
