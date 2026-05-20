#include "AnimGraphInstance.h"

#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimGraphCompiler.h"
#include "Animation/AnimGraphTypes.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationManager.h"
#include "Animation/Nodes/AnimNode_Base.h"
#include "Core/Log.h"
#include "Mesh/SkeletalMesh.h"
#include "Mesh/SkeletalMeshAsset.h"
#include "Serialization/Archive.h"

void UAnimGraphInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();

	USkeletalMesh* Mesh = GetSkeletalMesh();
	if (!Mesh) return;
	FSkeletalMesh* MeshAsset = Mesh->GetSkeletalMeshAsset();
	if (!MeshAsset || MeshAsset->Bones.empty()) return;

	// 자산 자동 생성 — 외부에서 SetGraphAsset 한 경우 그것을 사용.
	if (!GraphAsset)
	{
		GraphAsset = UObjectManager::Get().CreateObject<UAnimGraphAsset>(this);
		GraphAsset->InitializeDefault();
	}

	// Sequence resolve — DefaultSequencePath 로 LoadAnimation. 비어있거나 실패 시 nullptr.
	// SequencePlayer 는 Sequence==nullptr 일 때 ref pose 유지 — fallback 분기 별도 불필요.
	UAnimSequenceBase* Seq = nullptr;
	const FString PathStr = DefaultSequencePath.ToString();
	if (!PathStr.empty() && PathStr != "None")
	{
		Seq = FAnimationManager::Get().LoadAnimation(PathStr);
		if (!Seq)
		{
			UE_LOG("UAnimGraphInstance: 시퀀스 로드 실패. Path=%s", PathStr.c_str());
		}
	}

	if (FAnimGraphNode* SP = GraphAsset->FindFirstNodeOfType(EAnimGraphNodeType::SequencePlayer))
	{
		SP->SequenceRef = Seq;
	}

	FAnimNode_Base* Root = FAnimGraphCompiler::Compile(*GraphAsset, *this);
	if (!Root)
	{
		UE_LOG("UAnimGraphInstance: 컴파일 실패 — 트리 미설정, ref pose 유지.");
		return;
	}
	SetRootNode(Root);
}

void UAnimGraphInstance::Serialize(FArchive& Ar)
{
	// Editor-set 데모 파라미터만 — 자산(GraphAsset) 자체는 transient.
	// PIE Duplicate (UObject::Duplicate = Serialize 왕복) 가 path 만 라운드트립.
	// UCharacterAnimInstance 와 동일하게 Super::Serialize 호출 안 함 (ObjectName 직렬화 skip).
	FString PathStr = Ar.IsSaving() ? DefaultSequencePath.ToString() : FString();
	Ar << PathStr;
	if (Ar.IsLoading())
	{
		DefaultSequencePath = FSoftObjectPtr(PathStr);
	}
}
