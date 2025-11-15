#pragma once
#include "String.h"
#include "fbxsdk.h"
#include "SkeletalMesh.h"
#include "Material.h"
#include "UEContainer.h"

/**
 * FFbxMesh
 *
 * FBX 메시 데이터 추출 유틸리티
 * 정점, 인덱스, 스키닝, 머티리얼 추출
 *
 * UE5 Pattern: FFbxMesh (static utility functions)
 * Location: Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/FbxMesh.h
 *
 * Refactoring Phase 6 (v2.0): FBXLoader의 mesh 추출 로직을 중앙화
 * NOTE: 즉시 로딩 패턴 사용 (지연 로딩 미사용)
 */
struct FFbxMesh
{
	/**
	 * ExtractMeshFromNode
	 *
	 * FBX 노드를 재귀적으로 순회하며 메시 데이터 추출
	 *
	 * @param InNode - 순회할 FBX 노드
	 * @param MeshData - 메시 데이터를 저장할 구조체
	 * @param MaterialGroupIndexList - 머티리얼별 인덱스 리스트
	 * @param BoneToIndex - 본 노드 → 인덱스 매핑
	 * @param MaterialToIndex - 머티리얼 → 인덱스 매핑
	 * @param MaterialInfos - 추출된 머티리얼 정보 (bin 저장용)
	 * @param Scene - FBX Scene (TotalMatrix 계산용)
	 * @param CurrentFbxPath - 현재 FBX 파일 경로 (텍스처 경로 해석용)
	 * @param bForceFrontXAxis - JointPostConversionMatrix 플래그
	 *
	 * 처리 순서:
	 * 1. 노드의 모든 Attribute 순회
	 * 2. eMesh 타입 발견 시 머티리얼 슬롯 처리
	 * 3. ExtractMesh 호출하여 실제 메시 데이터 추출
	 * 4. 자식 노드 재귀 순회
	 */
	static void ExtractMeshFromNode(
		FbxNode* InNode,
		FSkeletalMeshData& MeshData,
		TMap<int32, TArray<uint32>>& MaterialGroupIndexList,
		TMap<FbxNode*, int32>& BoneToIndex,
		TMap<FbxSurfaceMaterial*, int32>& MaterialToIndex,
		TArray<FMaterialInfo>& MaterialInfos,
		FbxScene* Scene,
		const FString& CurrentFbxPath,
		bool bForceFrontXAxis);

	/**
	 * ExtractMesh
	 *
	 * FbxMesh에서 정점, 인덱스, 스키닝, 머티리얼 데이터 추출
	 *
	 * @param InMesh - FBX Mesh
	 * @param MeshData - 메시 데이터를 저장할 구조체
	 * @param MaterialGroupIndexList - 머티리얼별 인덱스 리스트
	 * @param BoneToIndex - 본 노드 → 인덱스 매핑
	 * @param MaterialSlotToIndex - 머티리얼 슬롯 → 인덱스 매핑 (최적화용)
	 * @param Scene - FBX Scene (TotalMatrix 계산용)
	 * @param DefaultMaterialIndex - 기본 머티리얼 인덱스 (머티리얼 없는 메시용)
	 * @param bForceFrontXAxis - JointPostConversionMatrix 플래그
	 *
	 * 처리 순서:
	 * 1. Deformer (Skin) 순회하여 스키닝 데이터 추출
	 * 2. Cluster에서 BindPose 행렬 계산 및 저장
	 * 3. ControlPoint → Bone 매핑 생성
	 * 4. TotalMatrix 계산 (GlobalTransform * GeometricTransform)
	 * 5. Polygon 순회하여 정점 데이터 추출
	 * 6. 탄젠트 자동 계산 (FBX에 없는 경우)
	 * 7. 인덱스 뒤집기 (CCW → CW 변환)
	 */
	static void ExtractMesh(
		FbxMesh* InMesh,
		FSkeletalMeshData& MeshData,
		TMap<int32, TArray<uint32>>& MaterialGroupIndexList,
		TMap<FbxNode*, int32>& BoneToIndex,
		TArray<int32> MaterialSlotToIndex,
		FbxScene* Scene,
		int32 DefaultMaterialIndex,
		bool bForceFrontXAxis);

private:
	/**
	 * ComputeSkeletalMeshTotalMatrix
	 *
	 * 스켈레탈 메시의 TotalMatrix 계산
	 * TotalMatrix = GlobalTransform * GeometricTransform
	 * DCC 툴에서 설정한 Pivot, Rotation Offset, Scaling Offset 포함
	 *
	 * UE5 Pattern: FbxSkeletalMeshImport.cpp Line 1607, 1624-1625
	 *
	 * @param MeshNode - 메시 노드
	 * @param Scene - FBX Scene (AnimationEvaluator 사용)
	 * @return TotalMatrix
	 */
	static FbxAMatrix ComputeSkeletalMeshTotalMatrix(
		FbxNode* MeshNode,
		FbxScene* Scene);
};
