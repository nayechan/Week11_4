#pragma once
#include "String.h"
#include "fbxsdk.h"

// Forward declarations
class UAnimSequence;
struct FBoneAnimationTrack;
struct FSkeleton;

/**
 * FFbxAnimation
 *
 * FBX 애니메이션 데이터 추출 유틸리티
 * 스켈레톤 본 애니메이션 커브 추출, 키프레임 베이킹
 *
 * UE5 Pattern: FFbxAnimation (static utility functions)
 * Location: Engine/Plugins/Interchange/Runtime/Source/Parsers/Fbx/Private/FbxAnimation.h
 *
 * Refactoring Phase 4 (v2.0): FBXLoader의 animation 추출 로직을 중앙화
 * NOTE: 즉시 로딩 패턴 사용 (지연 로딩 미사용)
 */
struct FFbxAnimation
{
	/**
	 * ExtractAnimation
	 *
	 * FBX AnimStack에서 애니메이션 데이터를 추출하여 UAnimSequence로 변환
	 * 즉시 로딩 패턴: UAnimSequence*를 직접 반환
	 *
	 * @param AnimStack - FBX AnimStack (타임스팬, 레이어 포함)
	 * @param TargetSkeleton - 타겟 스켈레톤 (본 매칭용)
	 * @param OutAnim - 애니메이션 데이터를 저장할 UAnimSequence
	 * @param JointOrientationMatrix - Joint 변환 행렬 (애니메이션 본 변환용)
	 *
	 * 처리 순서:
	 * 1. AnimLayer 및 타임스팬 추출
	 * 2. 프레임레이트 및 Duration 설정
	 * 3. 본별 애니메이션 트랙 재귀 추출
	 * 4. NumberOfKeys 계산
	 */
	static void ExtractAnimation(
		FbxAnimStack* AnimStack,
		const FSkeleton* TargetSkeleton,
		UAnimSequence* OutAnim,
		const FbxAMatrix& JointOrientationMatrix);

	/**
	 * ExtractBoneCurves
	 *
	 * FBX 씬을 재귀적으로 순회하며 Skeleton 노드를 찾아 애니메이션 커브 추출
	 * Depth-first 순회, 타겟 스켈레톤과 본 이름 매칭
	 *
	 * @param InNode - 현재 순회 중인 FBX 노드
	 * @param AnimLayer - FBX 애니메이션 레이어
	 * @param TargetSkeleton - 타겟 스켈레톤 (본 이름 매칭용)
	 * @param OutAnim - 애니메이션 트랙을 추가할 UAnimSequence
	 * @param JointOrientationMatrix - Joint 변환 행렬 (애니메이션 본 변환용)
	 * @param DebugBoneCount - 디버그 로깅용 카운터 (처음 3-4개만 자세히 로깅)
	 */
	static void ExtractBoneCurves(
		FbxNode* InNode,
		FbxAnimLayer* AnimLayer,
		const FSkeleton* TargetSkeleton,
		UAnimSequence* OutAnim,
		const FbxAMatrix& JointOrientationMatrix,
		int& DebugBoneCount);

	/**
	 * ExtractCurveData
	 *
	 * 특정 본의 애니메이션 커브에서 키프레임 데이터 추출
	 * UE5 패턴: EvaluateGlobalTransform 사용 (ConvertScene 적용된 Scene Graph 사용)
	 *
	 * @param BoneNode - 본 FBX 노드
	 * @param AnimLayer - 애니메이션 레이어
	 * @param TargetSkeleton - 타겟 스켈레톤
	 * @param OutTrack - 키프레임을 저장할 본 애니메이션 트랙
	 * @param JointOrientationMatrix - Joint 변환 행렬 (애니메이션 본 변환용)
	 * @param DebugBoneCount - 디버그 로깅용 카운터
	 *
	 * 처리 순서:
	 * 1. Translation, Rotation, Scale 커브 수집
	 * 2. 모든 커브에서 unique한 KeyTime 수집
	 * 3. 각 KeyTime에서 EvaluateGlobalTransform 호출
	 * 4. JointOrientationMatrix 적용
	 * 5. 부모 기준 로컬 변환 계산
	 * 6. Y-Flip 좌표 변환 적용
	 * 7. NaN/Inf 검증
	 */
	static void ExtractCurveData(
		FbxNode* BoneNode,
		FbxAnimLayer* AnimLayer,
		const FSkeleton* TargetSkeleton,
		FBoneAnimationTrack& OutTrack,
		const FbxAMatrix& JointOrientationMatrix,
		int& DebugBoneCount);

private:
	/**
	 * CollectUniqueKeyTimes
	 *
	 * 모든 애니메이션 커브에서 unique한 KeyTime 수집
	 *
	 * @param BoneNode - 본 FBX 노드
	 * @param AnimLayer - 애니메이션 레이어
	 * @param OutKeyTimes - 수집된 KeyTime 배열 (시간순 정렬됨)
	 * @return 수집된 커브들 (디버그용)
	 */
	static TArray<FbxAnimCurve*> CollectUniqueKeyTimes(
		FbxNode* BoneNode,
		FbxAnimLayer* AnimLayer,
		TArray<FbxTime>& OutKeyTimes);

	/**
	 * EvaluateLocalTransformAtTime
	 *
	 * 특정 시간에서 본의 로컬 변환 계산
	 * UE5 패턴: EvaluateGlobalTransform + 부모 역변환 + JointOrientationMatrix 적용
	 *
	 * @param BoneNode - 본 FBX 노드
	 * @param ParentNode - 부모 노드 (nullptr이면 root)
	 * @param Scene - FBX Scene (AnimationEvaluator 사용)
	 * @param KeyTime - 평가할 시간
	 * @param JointOrientationMatrix - Joint 변환 행렬 (애니메이션 본 변환용)
	 * @param bIsRootJoint - Root Joint 여부 (Parent에 JointOrientationMatrix 적용 여부 결정)
	 * @param OutPosition - 출력 위치 (Y-Flip 적용됨)
	 * @param OutRotation - 출력 회전 (Y-Flip 적용됨)
	 * @param OutScale - 출력 스케일
	 */
	static void EvaluateLocalTransformAtTime(
		FbxNode* BoneNode,
		FbxNode* ParentNode,
		FbxScene* Scene,
		FbxTime KeyTime,
		const FbxAMatrix& JointOrientationMatrix,
		bool bIsRootJoint,
		FVector& OutPosition,
		FQuat& OutRotation,
		FVector& OutScale);

	/**
	 * ValidateTransform
	 *
	 * 변환 값의 NaN/Inf 검증 및 수정
	 *
	 * @param Position - 위치 (수정 가능)
	 * @param Rotation - 회전 (수정 가능)
	 * @param Scale - 스케일 (수정 가능)
	 * @param KeyTime - 키프레임 시간 (로깅용)
	 * @return 유효하지 않은 값이 있었는지 여부
	 */
	static bool ValidateTransform(
		FVector& Position,
		FQuat& Rotation,
		FVector& Scale,
		FbxTime KeyTime);
};
