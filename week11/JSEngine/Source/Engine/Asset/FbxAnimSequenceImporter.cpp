#include "Asset/FbxAnimSequenceImporter.h"

#include "Animation/AnimDataModel.h"
#include "Animation/AnimSequence.h"
#include "Asset/FbxSceneImportContext.h"
#include "Asset/FbxTransformUtils.h"
#include "Asset/SkeletalMesh.h"
#include "Asset/Skeleton.h"
#include "Core/Logging/Log.h"
#include "Object/Object.h"

#include <fbxsdk.h>
#include <algorithm>
#include <cmath>
// Helper Function
static void BuildNodeNameMap(fbxsdk::FbxNode* Node, TMap<FString, fbxsdk::FbxNode*>& OutMap)
{
    if (!Node)
    {
        return;
    }

    OutMap[FString(Node->GetName())] = Node;

    for (int32 ChildIndex = 0; ChildIndex < Node->GetChildCount(); ++ChildIndex)
    {
        BuildNodeNameMap(Node->GetChild(ChildIndex), OutMap);
    }
}

static fbxsdk::FbxAnimStack* GetFirstAnimStack(fbxsdk::FbxScene* Scene)
{
    if (!Scene || Scene->GetSrcObjectCount<fbxsdk::FbxAnimStack>() <= 0)
    {
        return nullptr;
    }

    return Scene->GetSrcObject<fbxsdk::FbxAnimStack>(0);
}

static fbxsdk::FbxTimeSpan GetAnimationTimeSpan(fbxsdk::FbxScene* Scene, fbxsdk::FbxAnimStack* Stack)
{
    fbxsdk::FbxTimeSpan TimeSpan;

    if (Stack)
    {
        TimeSpan = Stack->GetLocalTimeSpan();
    }

    if (TimeSpan.GetDuration().GetSecondDouble() <= 0.0 && Scene)
    {
        Scene->GetGlobalSettings().GetTimelineDefaultTimeSpan(TimeSpan);
    }

    return TimeSpan;
}
static float GetSceneFrameRate(fbxsdk::FbxScene* Scene)
{
    if (!Scene)
    {
        return 30.0f;
    }

    const double Rate = fbxsdk::FbxTime::GetFrameRate(
        Scene->GetGlobalSettings().GetTimeMode());

    return Rate > 0.0 ? static_cast<float>(Rate) : 30.0f;
}
UAnimSequence* FFbxAnimSequenceImporter::LoadAnimSequence(const FString& Path, const FString& TargetSkeletalMeshPath, USkeletalMesh* TargetSkeletalMesh)
{	
	// 1. target mesh 검증
    // 2. FBX scene import
    // 3. anim stack 선택
    // 4. time span/frame rate/frame count 계산
    // 5. FBX node map 생성
    // 6. skeleton bone order 기준 track 생성
    // 7. frame sampling
    // 8. UAnimDataModel 생성
    // 9. UAnimSequence 생성
    return nullptr;
}

TArray<FString> FFbxAnimSequenceImporter::ListAnimStacks(const FString& Path)
{
    TArray<FString> Result;

    FFbxSceneImportContext Context;
    if (!Context.Import(Path))
    {
        return Result;
    }

    const int32 StackCount = Context.Scene->GetSrcObjectCount<fbxsdk::FbxAnimStack>();
    for (int32 StackIndex = 0; StackIndex < StackCount; ++StackIndex)
    {
        fbxsdk::FbxAnimStack* Stack =
            Context.Scene->GetSrcObject<fbxsdk::FbxAnimStack>(StackIndex);

        if (!Stack)
        {
            continue;
        }

        Result.push_back(FString(Stack->GetName()));
    }

    return Result;
}
//node traversal helper
//    anim stack listing
//        anim sequence loading
