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
	 * @param Scene - FBX Scene (Internal_GetRootSkeleton 호출용)
	 * @param RootNode - FBX 루트 노드
	 * @param MeshData - 스켈레톤 데이터를 저장할 FSkeletalMeshData
	 * @param BoneToIndex - FbxNode*를 본 인덱스로 매핑하는 맵 (출력)
	 * @param bCreatorIsBlender - Blender FBX 여부 (Armature 노드 감지용)
	 *
	 * 처리 순서:
	 * 1. 루트 노드의 모든 자식 노드를 재귀적으로 순회
	 * 2. eSkeleton 타입 노드를 찾아 FBone 생성
	 * 3. Blender Armature 노드 감지 및 스킵
	 * 4. 부모-자식 관계 설정
	 * 5. BoneToIndex 맵 생성 (메시 스키닝용)
	 * 6. BoneNameToIndex 맵 생성 (애니메이션용)
	 */
	static void ExtractSkeleton(
		FbxScene* Scene,
		FbxNode* RootNode,
		FSkeletalMeshData& MeshData,
		TMap<FbxNode*, int32>& BoneToIndex,
		bool bCreatorIsBlender);

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
	 * Internal_GetRootSkeleton
	 *
	 * UE5 Pattern: FFbxScene::Internal_GetRootSkeleton (FbxScene.cpp:768-814)
	 *
	 * 주어진 본 노드로부터 상위로 탐색하여 실제 root skeleton 노드를 찾음
	 * Blender FBX의 "Armature" dummy node를 특별 처리:
	 * - Blender는 "Armature" 이름의 eNull 노드를 root joint 부모로 생성
	 * - 이 노드는 100x scale + 90° rotation을 포함 (meter→cm + Z-up→Y-up 변환)
	 * - 일반 eNull 노드는 건너뛰지만, Blender Armature는 스켈레톤 경계로 간주
	 *
	 * @param SDKScene - FBX Scene
	 * @param Link - 시작 본 노드 (일반적으로 FbxCluster의 Link 노드)
	 * @param bCreatorIsBlender - Blender FBX 여부
	 * @return 실제 root skeleton 노드
	 *
	 * 알고리즘:
	 * 1. Link 노드부터 시작하여 부모 방향으로 탐색
	 * 2. 각 부모 노드의 타입 확인:
	 *    - eMesh: 메시 노드 → 건너뛰고 계속 상위 탐색
	 *    - eSkeleton: 스켈레톤 노드 → 건너뛰고 계속 상위 탐색
	 *    - eNull: 빈 노드 → Blender Armature인지 확인
	 *      * Blender Armature: 탐색 중단 (스켈레톤 경계)
	 *      * 일반 eNull: 건너뛰고 계속 상위 탐색
	 * 3. Scene Root 노드 도달 시 탐색 중단
	 * 4. 현재 RootBone 반환
	 *
	 * Blender Armature 감지 조건:
	 * - bCreatorIsBlender == true
	 * - 부모 노드 이름 == "Armature" (대소문자 무시)
	 * - 조부모 노드 == nullptr OR 조부모 == Scene Root Node
	 */
	static FbxNode* Internal_GetRootSkeleton(
		FbxScene* SDKScene,
		FbxNode* Link,
		bool bCreatorIsBlender);

	/**
	 * ExtractSkeletonRecursive
	 *
	 * 재귀적으로 FBX 노드를 순회하며 스켈레톤 본 추출
	 * Depth-first 순회, 부모 인덱스 전파
	 * Blender Armature 노드 감지 및 스킵
	 *
	 * @param Scene - FBX Scene (Internal_GetRootSkeleton 호출용)
	 * @param InNode - 현재 순회 중인 FBX 노드
	 * @param MeshData - 스켈레톤 데이터를 저장할 FSkeletalMeshData
	 * @param ParentNodeIndex - 부모 본 인덱스 (-1이면 루트)
	 * @param BoneToIndex - FbxNode*를 본 인덱스로 매핑하는 맵 (출력)
	 * @param bCreatorIsBlender - Blender FBX 여부 (Armature 노드 감지용)
	 */
	static void ExtractSkeletonRecursive(
		FbxScene* Scene,
		FbxNode* InNode,
		FSkeletalMeshData& MeshData,
		int32 ParentNodeIndex,
		TMap<FbxNode*, int32>& BoneToIndex,
		bool bCreatorIsBlender);
};
