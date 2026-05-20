#include "AnimGraphInstance.h"

#include "Animation/Graph/AnimGraphAsset.h"
#include "Animation/Graph/AnimGraphCompiler.h"
#include "Animation/Graph/AnimGraphTypes.h"
#include "Animation/Graph/AnimGraphManager.h"
#include "Animation/Sequence/AnimSequence.h"
#include "Animation/AnimationManager.h"
#include "Animation/Nodes/AnimNode_Base.h"
#include "Core/Log.h"
#include "Mesh/Skeletal/SkeletalMesh.h"
#include "Mesh/Skeletal/SkeletalMeshAsset.h"
#include "Serialization/Archive.h"

namespace
{
	UAnimSequenceBase* LoadByPath(const FString& Path)
	{
		if (Path.empty() || Path == "None") return nullptr;
		UAnimSequenceBase* Loaded = FAnimationManager::Get().LoadAnimation(Path);
		if (!Loaded)
		{
			UE_LOG("UAnimGraphInstance: 시퀀스 로드 실패. Path=%s", Path.c_str());
		}
		return Loaded;
	}
}

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

	// Version 강제 mismatch 로 첫 컴파일 트리거. (자산이 fresh load 라면 Version 이 0 이지만,
	// 다른 인스턴스가 이미 변경해 Version > 0 인 캐시 자산일 수도 있음 — 안전하게 강제.)
	CompiledAssetVersion = GraphAsset->GetVersion() - 1;
	RecompileTreeIfDirty();
}

void UAnimGraphInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	// in-editor live preview: 자산이 변경되면 다음 frame UpdateAnimation 의 RootNode->Update
	// 호출 전에 트리 재생성. 새 트리는 즉시 그 frame 부터 평가.
	RecompileTreeIfDirty();
}

void UAnimGraphInstance::RecompileTreeIfDirty()
{
	if (!GraphAsset) return;
	if (GraphAsset->GetVersion() == CompiledAssetVersion) return;

	// 노드별 SequenceRef 재해상 (per-node SequencePath > Instance.DefaultSequencePath > nullptr).
	const FString DefaultPathStr = DefaultSequencePath.ToString();
	for (FAnimGraphNode& Node : const_cast<TArray<FAnimGraphNode>&>(GraphAsset->GetNodes()))
	{
		if (Node.Type != EAnimGraphNodeType::SequencePlayer) continue;

		UAnimSequenceBase* Seq = LoadByPath(Node.SequencePath);
		if (!Seq) Seq = LoadByPath(DefaultPathStr);
		Node.SequenceRef = Seq;
	}

	// 기존 트리 폐기 — RootNode 먼저 nullptr 로 끊은 뒤 OwnedNodes clear.
	// (Update 호출 chain 이 다음 줄에 들어가므로 dangling 노출 위험 없음.)
	RootNode = nullptr;
	OwnedNodes.clear();

	FAnimNode_Base* Root = FAnimGraphCompiler::Compile(*GraphAsset, *this);
	if (!Root)
	{
		UE_LOG("UAnimGraphInstance: 컴파일 실패 — 트리 미설정, ref pose 유지.");
		CompiledAssetVersion = GraphAsset->GetVersion();
		return;
	}
	SetRootNode(Root);
	CompiledAssetVersion = GraphAsset->GetVersion();
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
