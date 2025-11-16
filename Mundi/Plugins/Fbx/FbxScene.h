#pragma once
#include "String.h"
#include "fbxsdk.h"
#include "SkeletalMesh.h"

// Forward declarations
struct FSkeletalMeshData;
struct FBone;
template<typename K, typename V> class TMap;

/**
 * FFbxScene
 *
 * FBX 씬 계층 구조 및 스켈레톤 추출 유틸리티
 * 재귀적 노드 순회, 본 계층 구조 생성, 단일 루트 본 보장
 *
 * UE5 Pattern: FFbxScene (static utility functions)
 * Location: Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/FbxScene.h
 *
 * Refactoring Phase 5 (v2.0): FBXLoader의 스켈레톤 추출 로직을 중앙화
 * NOTE: 즉시 로딩 패턴 사용 (지연 로딩 미사용)
 */
struct FFbxScene
{
	/**
	 * ExtractSkeleton
	 *
	 * FBX 씬에서 스켈레톤 계층 구조를 추출
	 * 루트 노드부터 재귀적으로 순회하며 eSkeleton 노드를 찾아 FBone 생성
	 *
	 * @param RootNode - FBX 루트 노드
	 * @param MeshData - 스켈레톤 데이터를 저장할 FSkeletalMeshData
	 * @param BoneToIndex - FbxNode*를 본 인덱스로 매핑하는 맵 (출력)
	 *
	 * 처리 순서:
	 * 1. 루트 노드의 모든 자식 노드를 재귀적으로 순회
	 * 2. eSkeleton 타입 노드를 찾아 FBone 생성
	 * 3. 부모-자식 관계 설정
	 * 4. BoneToIndex 맵 생성 (메시 스키닝용)
	 * 5. BoneNameToIndex 맵 생성 (애니메이션용)
	 */
	static void ExtractSkeleton(
		FbxNode* RootNode,
		FSkeletalMeshData& MeshData,
		TMap<FbxNode*, int32>& BoneToIndex);

	/**
	 * EnsureSingleRootBone
	 *
	 * 스켈레톤에 단일 루트 본을 보장
	 * 루트 본이 2개 이상이면 가상 루트 본 생성
	 *
	 * @param MeshData - 스켈레톤 데이터
	 *
	 * 처리 순서:
	 * 1. 루트 본 개수 확인 (ParentIndex == -1)
	 * 2. 루트 본이 2개 이상이면:
	 *    - "VirtualRoot" 이름의 가상 루트 본 생성
	 *    - 항등 행렬로 BindPose 초기화
	 *    - 기존 본들의 인덱스 +1
	 *    - 정점의 BoneIndices도 +1
	 */
	static void EnsureSingleRootBone(FSkeletalMeshData& MeshData);

private:
	/**
	 * ExtractSkeletonRecursive
	 *
	 * 재귀적으로 FBX 노드를 순회하며 스켈레톤 본 추출
	 * Depth-first 순회, 부모 인덱스 전파
	 *
	 * @param InNode - 현재 순회 중인 FBX 노드
	 * @param MeshData - 스켈레톤 데이터를 저장할 FSkeletalMeshData
	 * @param ParentNodeIndex - 부모 본 인덱스 (-1이면 루트)
	 * @param BoneToIndex - FbxNode*를 본 인덱스로 매핑하는 맵 (출력)
	 */
	static void ExtractSkeletonRecursive(
		FbxNode* InNode,
		FSkeletalMeshData& MeshData,
		int32 ParentNodeIndex,
		TMap<FbxNode*, int32>& BoneToIndex);
};
