#include "pch.h"
#include "AnimGraphInstance.h"
#include "SkeletalMeshComponent.h"
#include "SkeletalMesh.h"
#include "VertexData.h"
#include "GlobalConsole.h"

// ========================================
// UAnimGraphInstance 구현
// ========================================

UAnimNode* UAnimGraphInstance::CreateNodeByName(const FString& NodeTypeName)
{
	// 리플렉션 시스템을 통해 타입 이름으로 UClass 찾기
	// 예: "UAnimNode_SequencePlayer" → UAnimNode_SequencePlayer::StaticClass()
	UClass* NodeClass = UClass::FindClass(NodeTypeName.c_str());

	if (!NodeClass)
	{
		UE_LOG("AnimGraphInstance::CreateNodeByName - Unknown node type: %s", NodeTypeName.c_str());
		return nullptr;
	}

	// UAnimNode 상속 체크
	if (!NodeClass->IsChildOf(UAnimNode::StaticClass()))
	{
		UE_LOG("AnimGraphInstance::CreateNodeByName - Type '%s' is not a UAnimNode", NodeTypeName.c_str());
		return nullptr;
	}

	// 노드 생성
	UObject* NewObj = NewObject(NodeClass, this);
	UAnimNode* Node = Cast<UAnimNode>(NewObj);

	if (!Node)
	{
		UE_LOG("AnimGraphInstance::CreateNodeByName - Failed to cast %s to UAnimNode", NodeTypeName.c_str());
		ObjectFactory::DeleteObject(NewObj);
		return nullptr;
	}

	// 생명주기 관리 등록
	Nodes.Add(Node);

	UE_LOG("AnimGraphInstance::CreateNodeByName - Created node: %s", NodeTypeName.c_str());
	return Node;
}

void UAnimGraphInstance::NativeInitializeAnimation()
{
	// 부모 클래스 초기화
	UAnimInstance::NativeInitializeAnimation();

	// SkeletalMesh 및 Skeleton 유효성 검증
	if (!OwnerComponent || !OwnerComponent->GetSkeletalMesh())
	{
		UE_LOG("AnimGraphInstance::NativeInitializeAnimation - OwnerComponent or SkeletalMesh is null");
		return;
	}

	const USkeletalMesh* SkelMesh = OwnerComponent->GetSkeletalMesh();
	if (!SkelMesh->GetSkeletalMeshData())
	{
		UE_LOG("AnimGraphInstance::NativeInitializeAnimation - SkeletalMeshData is null");
		return;
	}

	const FSkeleton* Skel = SkelMesh->GetSkeletalMeshData()->Skeleton;

	// 모든 노드 초기화
	UE_LOG("AnimGraphInstance::NativeInitializeAnimation - Initializing %d nodes", Nodes.Num());

	for (UAnimNode* Node : Nodes)
	{
		if (Node)
		{
			Node->Initialize(Skel, this);
			UE_LOG("  - Initialized node: %s", Node->GetNodeName().c_str());
		}
	}

	UE_LOG("AnimGraphInstance::NativeInitializeAnimation - Complete");
}

void UAnimGraphInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 부모 클래스 업데이트
	UAnimInstance::NativeUpdateAnimation(DeltaSeconds);

	// 루트 노드 업데이트 (트리 전체 업데이트)
	if (RootNode)
	{
		RootNode->Update(DeltaSeconds);
	}
}

void UAnimGraphInstance::GetAnimationPose(FPoseContext& OutPose)
{
	if (RootNode)
	{
		// 루트 노드에 포즈 추출 위임
		// 트리 순회하며 자식 노드들도 Evaluate 호출됨
		// OutPose.AnimNotifies에 모든 Notify 누적됨
		RootNode->Evaluate(OutPose);
	}
	else
	{
		// RefPose 반환 (T-Pose)
		if (OwnerComponent && OwnerComponent->GetSkeletalMesh())
		{
			const USkeletalMesh* SkelMesh = OwnerComponent->GetSkeletalMesh();
			if (SkelMesh->GetSkeletalMeshData())
			{
				const FSkeleton* Skel = SkelMesh->GetSkeletalMeshData()->Skeleton;
				OutPose.SetNumBones(Skel->Bones.Num());

				// Identity transform으로 초기화됨 (FPoseContext::SetNumBones 내부)
			}
		}
	}
}
