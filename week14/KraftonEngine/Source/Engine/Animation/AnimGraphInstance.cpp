#include "AnimGraphInstance.h"

#include "Animation/AnimGraphAsset.h"
#include "Animation/AnimGraphCompiler.h"
#include "Animation/AnimGraphTypes.h"
#include "Animation/AnimGraphManager.h"
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

	// GraphAsset resolve 우선순위: 외부 SetGraphAsset > GraphAssetPath 로 디스크 로드 > 자동 transient.
	if (!GraphAsset)
	{
		const FString GraphPathStr = GraphAssetPath.ToString();
		if (!GraphPathStr.empty() && GraphPathStr != "None")
		{
			GraphAsset = FAnimGraphManager::Get().Load(GraphPathStr);
			if (!GraphAsset)
			{
				UE_LOG("UAnimGraphInstance: AnimGraph 로드 실패 → transient fallback. Path=%s", GraphPathStr.c_str());
			}
		}
	}
	if (!GraphAsset)
	{
		GraphAsset = UObjectManager::Get().CreateObject<UAnimGraphAsset>(this);
		GraphAsset->InitializeDefault();
	}

	// Sequence resolve 우선순위 (per-node):
	//   1) 노드의 SequencePath (자산에 박힘 — 그래프 편집 결과)
	//   2) UAnimGraphInstance::DefaultSequencePath (instance fallback)
	//   3) nullptr → SequencePlayer 가 ref pose 유지
	auto LoadByPath = [](const FString& Path) -> UAnimSequenceBase*
	{
		if (Path.empty() || Path == "None") return nullptr;
		UAnimSequenceBase* Loaded = FAnimationManager::Get().LoadAnimation(Path);
		if (!Loaded)
		{
			UE_LOG("UAnimGraphInstance: 시퀀스 로드 실패. Path=%s", Path.c_str());
		}
		return Loaded;
	};

	const FString DefaultPathStr = DefaultSequencePath.ToString();
	for (FAnimGraphNode& Node : const_cast<TArray<FAnimGraphNode>&>(GraphAsset->GetNodes()))
	{
		if (Node.Type != EAnimGraphNodeType::SequencePlayer) continue;

		UAnimSequenceBase* Seq = LoadByPath(Node.SequencePath);
		if (!Seq) Seq = LoadByPath(DefaultPathStr);
		Node.SequenceRef = Seq;
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
	// Editor-set 데모 파라미터만 — 런타임 GraphAsset 포인터는 transient (다음 InitializeAnimation
	// 에서 path 로 재해상). PIE Duplicate (UObject::Duplicate = Serialize 왕복) 가 path 들만 라운드트립.
	// UCharacterAnimInstance 와 동일하게 Super::Serialize 호출 안 함 (ObjectName 직렬화 skip).
	FString SeqPathStr   = Ar.IsSaving() ? DefaultSequencePath.ToString() : FString();
	FString GraphPathStr = Ar.IsSaving() ? GraphAssetPath.ToString()      : FString();
	Ar << SeqPathStr;
	Ar << GraphPathStr;
	if (Ar.IsLoading())
	{
		DefaultSequencePath = FSoftObjectPtr(SeqPathStr);
		GraphAssetPath      = FSoftObjectPtr(GraphPathStr);
	}
}
