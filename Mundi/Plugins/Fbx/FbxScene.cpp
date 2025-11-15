#include "pch.h"
#pragma warning(push)
#pragma warning(disable: 4244) // Disable double to float conversion warning for FBX SDK
#include "FbxScene.h"
#include "FbxHelper.h"
#include "SkeletalMesh.h"
#include "GlobalConsole.h"

// ========================================
// FBX 씬 계층 구조 및 스켈레톤 추출
// ========================================

/**
 * ExtractSkeleton
 *
 * FBX 씬에서 스켈레톤 계층 구조를 추출
 */
void FFbxScene::ExtractSkeleton(
	FbxScene* Scene,
	FbxNode* RootNode,
	FSkeletalMeshData& MeshData,
	TMap<FbxNode*, int32>& BoneToIndex,
	bool bCreatorIsBlender)
{
	if (!RootNode)
	{
		return;
	}

	// 루트 노드의 모든 자식 노드를 재귀적으로 순회
	// ParentIndex -1은 루트를 의미
	for (int Index = 0; Index < RootNode->GetChildCount(); Index++)
	{
		ExtractSkeletonRecursive(Scene, RootNode->GetChild(Index), MeshData, -1, BoneToIndex, bCreatorIsBlender);
	}
}

/**
 * Internal_GetRootSkeleton
 *
 * UE5 Pattern: FFbxScene::Internal_GetRootSkeleton (FbxScene.cpp:768-814)
 *
 * 주어진 본 노드로부터 상위로 탐색하여 실제 root skeleton 노드를 찾음
 * Blender FBX의 "Armature" dummy node를 특별 처리
 */
FbxNode* FFbxScene::Internal_GetRootSkeleton(
	FbxScene* SDKScene,
	FbxNode* Link,
	bool bCreatorIsBlender)
{
	FbxNode* RootBone = Link;

	// 부모 방향으로 탐색하며 실제 root skeleton 찾기
	while (RootBone && RootBone->GetParent())
	{
		// Blender Armature 감지
		bool bIsBlenderArmatureBone = false;
		if (bCreatorIsBlender)
		{
			FbxNode* ParentNode = RootBone->GetParent();
			FString RootBoneParentName(ParentNode->GetName());
			FbxNode* GrandFather = ParentNode->GetParent();

			// Blender Armature 조건:
			// 1. 부모 노드 이름이 "Armature" (대소문자 무시)
			// 2. 조부모가 nullptr OR Scene Root Node (top-level 노드)
			// stricmp를 사용한 대소문자 무시 비교
			bIsBlenderArmatureBone =
				(GrandFather == nullptr || GrandFather == SDKScene->GetRootNode()) &&
				(_stricmp(RootBoneParentName.c_str(), "armature") == 0);
		}

		FbxNodeAttribute* Attr = RootBone->GetParent()->GetNodeAttribute();
		if (Attr &&
			(Attr->GetAttributeType() == FbxNodeAttribute::eMesh ||
			 Attr->GetAttributeType() == FbxNodeAttribute::eSkeleton ||
			 (Attr->GetAttributeType() == FbxNodeAttribute::eNull && !bIsBlenderArmatureBone)) &&
			RootBone->GetParent() != SDKScene->GetRootNode())
		{
			// 일반 노드 (eMesh, eSkeleton, eNull)는 건너뛰고 계속 상위 탐색
			// 단, Blender Armature eNull은 건너뛰지 않음 (스켈레톤 경계)
			RootBone = RootBone->GetParent();
		}
		else
		{
			// 탐색 중단 조건:
			// - Blender Armature 노드 도달
			// - Scene Root 노드 도달
			// - Attribute 없는 노드
			break;
		}
	}

	return RootBone;
}

/**
 * ExtractSkeletonRecursive
 *
 * 재귀적으로 FBX 노드를 순회하며 스켈레톤 본 추출
 */
void FFbxScene::ExtractSkeletonRecursive(
	FbxScene* Scene,
	FbxNode* InNode,
	FSkeletalMeshData& MeshData,
	int32 ParentNodeIndex,
	TMap<FbxNode*, int32>& BoneToIndex,
	bool bCreatorIsBlender)
{
	// Skeleton은 계층구조까지 표현해야하므로 깊이 우선 탐색, ParentNodeIndex 명시.
	int32 BoneIndex = ParentNodeIndex;
	for (int Index = 0; Index < InNode->GetNodeAttributeCount(); Index++)
	{
		FbxNodeAttribute* Attribute = InNode->GetNodeAttributeByIndex(Index);
		if (!Attribute)
		{
			continue;
		}

		if (Attribute->GetAttributeType() == FbxNodeAttribute::eSkeleton)
		{
			FBone BoneInfo{};

			BoneInfo.Name = FFbxHelper::GetFbxObjectName(InNode, false);

			// ========================================
			// UE5 Pattern: Blender Armature 감지
			// ========================================
			// Blender FBX는 "Armature" eNull 노드를 skeleton root 부모로 생성
			// 이 노드는 100x scale + 90° rotation을 포함
			// Internal_GetRootSkeleton을 사용하여 실제 skeleton root 판단
			FbxNode* SkeletonRoot = Internal_GetRootSkeleton(Scene, InNode, bCreatorIsBlender);

			// 만약 SkeletonRoot가 InNode와 같다면, 이 노드가 실제 skeleton root
			// (Blender Armature 위로 올라가지 않음)
			int32 ActualParentIndex = ParentNodeIndex;
			if (SkeletonRoot == InNode)
			{
				// 이 노드는 skeleton의 실제 root - ParentIndex를 -1로 설정
				ActualParentIndex = -1;
				UE_LOG("[FFbxScene] Detected skeleton root '%s' (Blender Armature parent excluded)",
				       BoneInfo.Name.c_str());
			}

			BoneInfo.ParentIndex = ActualParentIndex;

			// 뼈 리스트에 추가
			MeshData.Skeleton.Bones.Add(BoneInfo);

			// 뼈 인덱스 우리가 정해줌(방금 추가한 마지막 원소)
			BoneIndex = MeshData.Skeleton.Bones.Num() - 1;

			// 뼈 이름으로 인덱스 서치 가능하게 함.
			MeshData.Skeleton.BoneNameToIndex.Add(BoneInfo.Name, BoneIndex);

			// 매시 로드할때 써야되서 맵에 인덱스 저장
			BoneToIndex.Add(InNode, BoneIndex);
			// 뼈가 노드 하나에 여러개 있는 경우는 없음. 말이 안되는 상황임.
			break;
		}
	}
	for (int Index = 0; Index < InNode->GetChildCount(); Index++)
	{
		// 깊이 우선 탐색 부모 인덱스 설정(InNode가 eSkeleton이 아니면 기존 부모 인덱스가 넘어감(BoneIndex = ParentNodeIndex)
		ExtractSkeletonRecursive(Scene, InNode->GetChild(Index), MeshData, BoneIndex, BoneToIndex, bCreatorIsBlender);
	}
}

/**
 * EnsureSingleRootBone
 *
 * 스켈레톤에 단일 루트 본을 보장
 * 루트 본이 2개 이상이면 가상 루트 본 생성
 */
void FFbxScene::EnsureSingleRootBone(FSkeletalMeshData& MeshData)
{
	if (MeshData.Skeleton.Bones.IsEmpty())
		return;

	// 루트 본 개수 세기
	TArray<int32> RootBoneIndices;
	for (int32 i = 0; i < MeshData.Skeleton.Bones.size(); ++i)
	{
		if (MeshData.Skeleton.Bones[i].ParentIndex == -1)
		{
			RootBoneIndices.Add(i);
		}
	}

	// 루트 본이 2개 이상이면 가상 루트 생성
	if (RootBoneIndices.Num() > 1)
	{
		// 가상 루트 본 생성
		FBone VirtualRoot;
		VirtualRoot.Name = "VirtualRoot";
		VirtualRoot.ParentIndex = -1;

		// 항등 행렬로 초기화 (원점에 위치, 회전/스케일 없음)
		VirtualRoot.BindPose = FMatrix::Identity();
		VirtualRoot.InverseBindPose = FMatrix::Identity();

		// 가상 루트를 배열 맨 앞에 삽입
		MeshData.Skeleton.Bones.Insert(VirtualRoot, 0);

		// 기존 본들의 인덱스가 모두 +1 씩 밀림
		// 모든 본의 ParentIndex 업데이트
		for (int32 i = 1; i < MeshData.Skeleton.Bones.size(); ++i)
		{
			if (MeshData.Skeleton.Bones[i].ParentIndex >= 0)
			{
				MeshData.Skeleton.Bones[i].ParentIndex += 1;
			}
			else // 원래 루트 본들
			{
				MeshData.Skeleton.Bones[i].ParentIndex = 0; // 가상 루트를 부모로 설정
			}
		}

		// Vertex의 BoneIndex도 모두 +1 해줘야 함
		for (auto& Vertex : MeshData.Vertices)
		{
			for (int32 i = 0; i < 4; ++i)
			{
				Vertex.BoneIndices[i] += 1;
			}
		}

		UE_LOG("UFbxLoader: Created virtual root bone. Found %d root bones.", RootBoneIndices.Num());
	}
}

#pragma warning(pop) // Restore warning state
